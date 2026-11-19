cmake_minimum_required(VERSION 3.20)

# 基于 MiniLM 的短视频个性化智能筛选系统
# UI 表现层（Qt 悬浮球） + 核心算法静态库（VideoFilteringCore）双层解耦架构
project(QtFloatingBall VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ========== 目录约定 ==========
set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/VideoFilteringCore")
set(ONNX_DIR "${CORE_DIR}/onnx/onnxruntime-win-x64-1.14.1")

if(NOT EXISTS "${ONNX_DIR}/lib/onnxruntime.lib")
    message(FATAL_ERROR "未找到 ONNX Runtime SDK：${ONNX_DIR}/lib/onnxruntime.lib")
endif()
if(NOT EXISTS "${CORE_DIR}/include/sentencepiece.lib")
    message(FATAL_ERROR "未找到 SentencePiece 静态库：${CORE_DIR}/include/sentencepiece.lib")
endif()

# ========== 查找 Qt（6 优先，兼容 Qt5） ==========
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
    set(QT_PREFIX Qt5)
    message(STATUS "使用 Qt5")
else()
    set(QT_PREFIX Qt6)
    message(STATUS "使用 Qt6")
endif()

# ========== 核心引擎层：VideoFilteringCore（静态库，无 Qt 依赖） ==========
add_library(VideoFilteringCore STATIC
    ${CORE_DIR}/application/ApplicationService.cpp
    ${CORE_DIR}/application/ConfigurationManager.cpp
    ${CORE_DIR}/application/FilterEngine.cpp
    ${CORE_DIR}/application/StateManager.cpp
    ${CORE_DIR}/business_operations.cpp
    ${CORE_DIR}/clean/cleaners.cpp
    ${CORE_DIR}/clean/parser.cpp
    ${CORE_DIR}/core/similarity_policy.cpp
    ${CORE_DIR}/model/comment_loader.cpp
    ${CORE_DIR}/model/onnx_infer.cpp
    ${CORE_DIR}/model/string_util.cpp
    ${CORE_DIR}/model/text_preproc.cpp
    ${CORE_DIR}/pch.cpp
    ${CORE_DIR}/utils/utils.cpp
    ${CORE_DIR}/utils/utils_clipboard.cpp
    ${CORE_DIR}/utils/utils_file.cpp
    ${CORE_DIR}/utils/utils_input.cpp
    ${CORE_DIR}/utils/utils_log.cpp
    ${CORE_DIR}/utils/utils_math.cpp
    ${CORE_DIR}/VideoFilteringCore.cpp
)

# 对外只暴露接口目录：UI 层通过 IApplicationController 访问核心能力
target_include_directories(VideoFilteringCore PUBLIC
    ${CORE_DIR}/application
)
target_include_directories(VideoFilteringCore PRIVATE
    ${CORE_DIR}
    ${CORE_DIR}/include
    ${CORE_DIR}/nlohmann
    ${CORE_DIR}/core
    ${CORE_DIR}/clean
    ${CORE_DIR}/model
    ${CORE_DIR}/utils
    ${ONNX_DIR}/include
)

# 源码为 UTF-8 无 BOM，必须显式声明编码，否则 MSVC 按本地代码页(936)解析会截断中文字符串。
# 其他宏（_CRT_SECURE_NO_WARNINGS / NOMINMAX 等）由各源文件自行定义，此处不重复全局声明，
# 否则会与源文件内的 #define 冲突（C4005），且全局 NOMINMAX 会破坏代码中裸 max() 的调用。
target_compile_options(VideoFilteringCore PRIVATE /utf-8)

# ========== UI 表现层：Qt 悬浮球可执行程序 ==========
add_executable(${PROJECT_NAME} WIN32
    main_videofiltering.cpp
    VideoFilteringWidget.cpp
    VideoFilteringWidget.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    VideoFilteringCore
    ${QT_PREFIX}::Core
    ${QT_PREFIX}::Gui
    ${QT_PREFIX}::Widgets
    "${ONNX_DIR}/lib/onnxruntime.lib"
    "${CORE_DIR}/include/sentencepiece.lib"
    user32
)

# ========== 构建后把运行时 DLL 复制到输出目录 ==========
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNX_DIR}/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNX_DIR}/lib/onnxruntime_providers_shared.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "复制 ONNX Runtime 运行时 DLL 到输出目录"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /Zc:__cplusplus)
endif()

# ========== 部署说明 ==========
# models/model.onnx 与 models/sentencepiece.bpe.model 体积较大（合计约 450MB），
# 不纳入版本库，请按 models/README.md 说明自行获取。
