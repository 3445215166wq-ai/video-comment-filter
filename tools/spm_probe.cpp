// spm_probe.cpp
// 用途：加载一个 SentencePiece .model，打印特殊词元信息，并对给定文本文件逐行分词，
//      输出「piece 序列」与「id 序列」。
//
// 存在的意义：在不依赖 Python 包的前提下，用**项目实际链接的那份 sentencepiece.lib**
// 验证重生成的 .model 是否 (a) 可加载 (b) 分词行为与原文件一致。
//
// 构建与用法见 tools/CMakeLists.txt 与本仓库 docs/TOKENIZER_ALIGNMENT.md
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "sentencepiece_processor.h"

// 以宽字符路径读取文件全部字节，避免 SentencePiece 的窄字符路径在
// 含中文等非 ASCII 字符的目录下失效（详见 text_preproc.cpp 中的同类处理）。
static bool ReadAllBytesW(const std::wstring& path, std::string* out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) return false;
    out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !out->empty();
}

static std::wstring Widen(const char* s) {
    // 仅用于把命令行参数转成宽字符，测试环境为 Windows；非 ASCII 参数请用文件传入
    std::wstring w;
    while (*s) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*s++)));
    return w;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        std::fwprintf(stderr, L"usage: spm_probe <model> <textfile-utf8>\n");
        return 2;
    }

    std::string blob;
    if (!ReadAllBytesW(argv[1], &blob)) {
        std::fwprintf(stderr, L"[ERROR] cannot read model: %ls\n", argv[1]);
        return 3;
    }

    sentencepiece::SentencePieceProcessor sp;
    auto st = sp.LoadFromSerializedProto(blob);
    if (!st.ok()) {
        std::fwprintf(stderr, L"[ERROR] LoadFromSerializedProto failed: %hs\n", st.ToString().c_str());
        return 4;
    }

    std::printf("piece_size=%d\n", sp.GetPieceSize());
    const char* names[] = {"<s>", "</s>", "<unk>", "<pad>", "<mask>"};
    for (const char* n : names) {
        int id = sp.PieceToId(n);
        std::printf("id(%-6s)=%d  unk=%d ctrl=%d\n", n, id, (int)sp.IsUnknown(id), (int)sp.IsControl(id));
    }
    std::printf("--- begin tokenize ---\n");

    std::ifstream tf(argv[2], std::ios::binary);
    if (!tf.is_open()) {
        std::fwprintf(stderr, L"[ERROR] cannot read text file: %ls\n", argv[2]);
        return 5;
    }

    std::string line;
    while (std::getline(tf, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> pieces;
        std::vector<int> ids;
        sp.Encode(line, &pieces);
        sp.Encode(line, &ids);

        std::printf("PIECES\t");
        for (size_t i = 0; i < pieces.size(); ++i)
            std::printf("%s%s", i ? "|" : "", pieces[i].c_str());
        std::printf("\nIDS\t");
        for (size_t i = 0; i < ids.size(); ++i)
            std::printf("%s%d", i ? "," : "", ids[i]);
        std::printf("\n");
    }
    std::printf("--- end ---\n");
    return 0;
}
