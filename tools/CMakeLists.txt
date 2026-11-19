cmake_minimum_required(VERSION 3.20)

# 独立的验证工具：用项目实际链接的 sentencepiece.lib 检查 .model 文件
# 与主工程解耦，不影响 QtFloatingBall 的构建。
project(spm_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../VideoFilteringCore")

if(NOT EXISTS "${CORE_DIR}/include/sentencepiece.lib")
    message(FATAL_ERROR "未找到 ${CORE_DIR}/include/sentencepiece.lib")
endif()

add_executable(spm_probe spm_probe.cpp)

target_include_directories(spm_probe PRIVATE "${CORE_DIR}/include")
target_link_libraries(spm_probe PRIVATE "${CORE_DIR}/include/sentencepiece.lib")

if(MSVC)
    target_compile_options(spm_probe PRIVATE /utf-8)
endif()
