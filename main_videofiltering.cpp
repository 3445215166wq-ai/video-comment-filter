/**
 * main_videofiltering.cpp
 * 视频筛选悬浮球控制面板 - 程序入口
 *
 * 使用方法：
 * 1. 将 IApplicationController.h 放入项目目录
 * 2. 链接 VideoFiltering 核心库
 * 3. 编译运行
 */
 // AI辅助生成：DeepSeek-R1, 2026-4-22
#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <memory>
#include "VideoFilteringWidget.h"
#include "IApplicationController.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 设置应用程序属性
    app.setApplicationName("VideoFilteringWidget");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("VideoFiltering");

    // 设置高 DPI 支持
    app.setStyle("Fusion");

    // ========== 初始化控制器 ==========
    std::wstring exeDir = QCoreApplication::applicationDirPath().toStdWString();
    std::wstring dataDir = exeDir + L"/data";

    auto uniqueCtrl = CreateApplicationController(exeDir, dataDir);
    if (!uniqueCtrl) {
        QMessageBox::critical(nullptr, "错误",
            "初始化失败：无法创建应用控制器。\n\n"
            "请确保以下条件满足：\n"
            "1. VideoFiltering 核心库已正确安装\n"
            "2. 程序目录具有写入权限\n"
            "3. 必要的依赖文件存在");
        return -1;
    }

    // 转为 shared_ptr 便于在 Qt 对象间共享
    std::shared_ptr<IApplicationController> controller(std::move(uniqueCtrl));

    // ========== 创建悬浮球窗口 ==========
    VideoFilteringWidget widget(controller);
    widget.show();

    return app.exec();
}