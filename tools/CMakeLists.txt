cmake_minimum_required(VERSION 3.20)

# 验证与测试工具（随仓库分发）
#   与主工程解耦，不影响 QtFloatingBall 的构建。
#   构建：cmake -S tools -B <build> -G "Visual Studio 18 2026" -A x64
#         cmake --build <build> --config Release
project(vcf_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../VideoFilteringCore")
set(ONNX_DIR "${CORE_DIR}/onnx/onnxruntime-win-x64-1.14.1")

if(NOT EXISTS "${CORE_DIR}/include/sentencepiece.lib")
    message(FATAL_ERROR "未找到 ${CORE_DIR}/include/sentencepiece.lib")
endif()

# ========== 1. 分词探针：验证 .model 文件的特殊词元与逐句分词结果 ==========
add_executable(spm_probe spm_probe.cpp)
target_include_directories(spm_probe PRIVATE "${CORE_DIR}/include")
target_link_libraries(spm_probe PRIVATE "${CORE_DIR}/include/sentencepiece.lib")

# ========== 2. 决策矩阵断言（纯逻辑，不需要模型）==========
add_executable(test_policy
    test_policy.cpp
    ${CORE_DIR}/core/similarity_policy.cpp
    ${CORE_DIR}/utils/utils_math.cpp
    ${CORE_DIR}/utils/utils_log.cpp
)
target_include_directories(test_policy PRIVATE
    ${CORE_DIR} ${CORE_DIR}/core ${CORE_DIR}/clean ${CORE_DIR}/model ${CORE_DIR}/utils ${CORE_DIR}/include)

# ========== 3. 黄金样例断言（需要模型与分词器，放 bin/ 下运行）==========
add_executable(test_golden
    test_golden.cpp
    ${CORE_DIR}/model/onnx_infer.cpp
    ${CORE_DIR}/model/text_preproc.cpp
    ${CORE_DIR}/model/string_util.cpp
    ${CORE_DIR}/utils/utils.cpp
    ${CORE_DIR}/utils/utils_file.cpp
    ${CORE_DIR}/utils/utils_log.cpp
    ${CORE_DIR}/utils/utils_math.cpp
)
target_include_directories(test_golden PRIVATE
    ${CORE_DIR} ${CORE_DIR}/core ${CORE_DIR}/clean ${CORE_DIR}/model ${CORE_DIR}/utils
    ${CORE_DIR}/include ${ONNX_DIR}/include)
target_link_libraries(test_golden PRIVATE
    "${ONNX_DIR}/lib/onnxruntime.lib"
    "${CORE_DIR}/include/sentencepiece.lib")

if(MSVC)
    target_compile_options(spm_probe PRIVATE /utf-8)
    target_compile_options(test_policy PRIVATE /utf-8)
    target_compile_options(test_golden PRIVATE /utf-8)
endif()
