#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 HuggingFace tokenizer.json 的原始顺序重建 SentencePiece .model 文件。

背景
----
常见的 "tokenizer.json -> sentencepiece .model" 转换工具会把特殊词元按
SentencePiece 自身默认约定重排（unk=0, bos=1, eos=2，且不带 pad），而
HuggingFace 的词表顺序通常是 <s>=0, <pad>=1, </s>=2, <unk>=3。

两者对"特殊词元占几个位置"的约定不同，导致其后**所有正常词元的 id 整体前移/后移**。
如果下游模型的 embedding 仍按原顺序训练，则 token id 与 embedding 行号错位，
语义结果完全失效，且全程不报错（典型 silent failure）。

本脚本做的事
------------
1. 以 tokenizer.json 的 piece 顺序为准（那才是模型训练时用的顺序）
2. 从现有 .model 中按 piece 字符串取出 score —— 保证分词切分行为不变（同一份 piece 与 score）
3. 显式写入 trainer_spec 的 unk_id / bos_id / eos_id / pad_id
4. normalizer_spec 原样保留（含 precompiled charsmap），保证归一化行为不变
5. 生成后自动回读校验

用法
----
    python tools/rebuild_sentencepiece_model.py \
        --tokenizer-json 原版/tokenizer.json \
        --spm models/sentencepiece.bpe.model \
        --out models/sentencepiece.bpe.model.fixed

    # 校验通过后替换
    mv models/sentencepiece.bpe.model.fixed models/sentencepiece.bpe.model

依赖：仅 Python 标准库（内置 protobuf 读写，无需安装 sentencepiece / protobuf）
"""

import argparse
import json
import struct
import sys

# SentencePiece.ModelProto.SentencePiece.Type
T_NORMAL = 1
T_UNKNOWN = 2
T_CONTROL = 3

SPECIAL_CONTROL = ("<s>", "</s>", "<pad>", "<mask>")
SPECIAL_UNKNOWN = ("<unk>",)

# 需要显式指定的 trainer_spec 字段号（由解析现有 .model 实测确认）
F_MODEL_TYPE = 3
F_VOCAB_SIZE = 4
F_UNK_ID = 40
F_BOS_ID = 41
F_EOS_ID = 42
F_PAD_ID = 43

F_PIECES = 1
F_TRAINER_SPEC = 2
F_NORMALIZER_SPEC = 3


# ---------------- protobuf 基础读写 ----------------

def read_varint(buf, pos):
    result = 0
    shift = 0
    while True:
        b = buf[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        shift += 7
        if not (b & 0x80):
            return result, pos


def write_varint(value):
    if value < 0:  # int32/int64 负数按 64 位补码编码
        value += 1 << 64
    out = bytearray()
    while True:
        b = value & 0x7F
        value >>= 7
        if value:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def iter_fields(buf, start, end):
    """产出 (field_number, wire_type, value_or_slice)"""
    pos = start
    while pos < end:
        key, pos = read_varint(buf, pos)
        fn, wt = key >> 3, key & 7
        if wt == 0:
            v, pos = read_varint(buf, pos)
            yield fn, wt, v
        elif wt == 2:
            ln, pos = read_varint(buf, pos)
            yield fn, wt, buf[pos:pos + ln]
            pos += ln
        elif wt == 5:
            yield fn, wt, buf[pos:pos + 4]
            pos += 4
        elif wt == 1:
            yield fn, wt, buf[pos:pos + 8]
            pos += 8
        else:
            raise ValueError("unsupported wire type %d at %d" % (wt, pos))


def enc_varint_field(fn, value):
    return write_varint((fn << 3) | 0) + write_varint(value)


def enc_bytes_field(fn, payload):
    return write_varint((fn << 3) | 2) + write_varint(len(payload)) + payload


def enc_float_field(fn, raw4):
    return write_varint((fn << 3) | 5) + raw4


# ---------------- 解析现有 .model ----------------

def parse_model(data):
    pieces = []          # list of (piece, score, type)
    trainer_fields = []  # list of (fn, wt, payload)
    normalizer_raw = None

    for fn, wt, val in iter_fields(data, 0, len(data)):
        if fn == F_PIECES and wt == 2:
            piece, score, ptype = None, 0.0, T_NORMAL
            for f2, w2, v2 in iter_fields(val, 0, len(val)):
                if f2 == 1 and w2 == 2:
                    piece = v2.decode("utf-8")
                elif f2 == 2 and w2 == 5:
                    score = struct.unpack("<f", v2)[0]
                elif f2 == 3 and w2 == 0:
                    ptype = v2
            if piece is None:
                raise ValueError("piece without string")
            pieces.append((piece, score, ptype))
        elif fn == F_TRAINER_SPEC and wt == 2:
            trainer_fields = list(iter_fields(val, 0, len(val)))
        elif fn == F_NORMALIZER_SPEC and wt == 2:
            normalizer_raw = val

    if normalizer_raw is None:
        raise ValueError("normalizer_spec not found")
    return pieces, trainer_fields, normalizer_raw


def load_tokenizer_order(path):
    with open(path, "r", encoding="utf-8") as f:
        tk = json.load(f)
    vocab = tk["model"]["vocab"]
    if isinstance(vocab, dict):
        order = [None] * len(vocab)
        for piece, idx in vocab.items():
            order[idx] = piece
    else:
        # Unigram: list of [piece, score]
        order = [p[0] if isinstance(p, (list, tuple)) else p for p in vocab]
    return order


# ---------------- 重建 ----------------

def build(pieces, trainer_fields, normalizer_raw, order):
    old_score = {}
    for piece, score, _ptype in pieces:
        old_score.setdefault(piece, score)

    out_pieces = []
    missing = []
    for piece in order:
        if piece in SPECIAL_UNKNOWN:
            score, ptype = 0.0, T_UNKNOWN
        elif piece in SPECIAL_CONTROL:
            score, ptype = 0.0, T_CONTROL
        else:
            if piece in old_score:
                score, ptype = old_score[piece], T_NORMAL
            else:
                score, ptype = -30.0, T_NORMAL   # 原 spm 中不存在，给一个很低的分数
                missing.append(piece)
        out_pieces.append((piece, score, ptype))

    index = {p: i for i, (p, _s, _t) in enumerate(out_pieces)}
    for need in ("<s>", "</s>", "<unk>", "<pad>"):
        if need not in index:
            raise ValueError("tokenizer.json 缺少必需特殊词元 %s" % need)

    patch = {
        F_MODEL_TYPE: 1,                  # UNIGRAM
        F_VOCAB_SIZE: len(out_pieces),
        F_UNK_ID: index["<unk>"],
        F_BOS_ID: index["<s>"],
        F_EOS_ID: index["</s>"],
        F_PAD_ID: index["<pad>"],
    }

    buf = bytearray()
    for piece, score, ptype in out_pieces:
        body = enc_bytes_field(1, piece.encode("utf-8"))
        body += enc_float_field(2, struct.pack("<f", score))
        body += enc_varint_field(3, ptype)
        buf += enc_bytes_field(F_PIECES, body)

    ts = bytearray()
    seen = set()
    for fn, wt, payload in trainer_fields:
        if fn in patch:
            ts += enc_varint_field(fn, patch[fn])
            seen.add(fn)
        elif wt == 0:
            ts += enc_varint_field(fn, payload)
        elif wt == 2:
            ts += enc_bytes_field(fn, payload)
        elif wt == 5:
            ts += enc_float_field(fn, payload)
        elif wt == 1:
            ts += write_varint((fn << 3) | 1) + payload
    for fn in sorted(patch):
        if fn not in seen:
            ts += enc_varint_field(fn, patch[fn])

    buf += enc_bytes_field(F_TRAINER_SPEC, bytes(ts))
    buf += enc_bytes_field(F_NORMALIZER_SPEC, normalizer_raw)
    return bytes(buf), index, missing


def verify(path, order):
    data = open(path, "rb").read()
    pieces, trainer_fields, normalizer_raw = parse_model(data)
    got = [p for p, _s, _t in pieces]
    assert got == order, "piece order mismatch: got %d, want %d" % (len(got), len(order))

    ts = {}
    for fn, wt, payload in trainer_fields:
        if wt == 0:
            ts[fn] = payload
    expect = {
        F_UNK_ID: got.index("<unk>"),
        F_BOS_ID: got.index("<s>"),
        F_EOS_ID: got.index("</s>"),
        F_PAD_ID: got.index("<pad>"),
        F_VOCAB_SIZE: len(got),
        F_MODEL_TYPE: 1,
    }
    for fn, want in expect.items():
        if ts.get(fn) != want:
            raise AssertionError("trainer_spec field %d = %r, want %r" % (fn, ts.get(fn), want))

    types = {p: t for p, _s, t in pieces}
    for p in SPECIAL_CONTROL:
        assert types.get(p) == T_CONTROL, "%s type = %r, want CONTROL" % (p, types.get(p))
    assert types.get("<unk>") == T_UNKNOWN

    print("  verify: pieces=%d  normalizer_spec=%d bytes  OK" % (len(got), len(normalizer_raw)))
    print("  verify: unk_id=%d bos_id=%d eos_id=%d pad_id=%d"
          % (ts[F_UNK_ID], ts[F_BOS_ID], ts[F_EOS_ID], ts[F_PAD_ID]))
    return len(got)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tokenizer-json", required=True)
    ap.add_argument("--spm", required=True, help="现有（顺序可能已错位的）.model，用作 score 来源")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    print("[1/4] 解析现有 spm: %s" % args.spm)
    pieces, trainer_fields, normalizer_raw = parse_model(open(args.spm, "rb").read())
    print("       pieces=%d, trainer_spec fields=%d, normalizer=%d bytes"
          % (len(pieces), len(trainer_fields), len(normalizer_raw)))

    print("[2/4] 读取 tokenizer.json 顺序: %s" % args.tokenizer_json)
    order = load_tokenizer_order(args.tokenizer_json)
    print("       vocab entries=%d, head=%r" % (len(order), order[:6]))

    print("[3/4] 重建并写出: %s" % args.out)
    data, index, missing = build(pieces, trainer_fields, normalizer_raw, order)
    open(args.out, "wb").write(data)
    print("       写出 %d bytes；原 spm 中缺失的 piece 数=%d" % (len(data), len(missing)))
    if missing:
        print("       注意：以下 piece 原 spm 中没有，已给 -30 低分：%r" % missing[:10])

    print("[4/4] 回读校验")
    verify(args.out, order)
    print("DONE")


if __name__ == "__main__":
    sys.exit(main())
