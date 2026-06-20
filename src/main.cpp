#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QtPlugin>
#include "control_panel.h"
#include "overlay_window.h"
#include "map_status_detector.h"
#include "config_manager.h"
#include "logger.h"

// 如果是静态编译，确保包含插件导入
#ifdef QT_STATIC
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

// 从 exe 自身资源中加载嵌入的应用图标（resources/app.rc 中的 IDI_APPICON），
// 这样不依赖外部 .ico 文件，exe 单文件即可在任务栏/标题栏显示图标。
static QIcon loadAppIcon() {
    HICON hIcon = LoadIconW(GetModuleHandleW(NULL), L"IDI_APPICON");
    if (!hIcon) hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    return QIcon(QPixmap::fromImage(QImage::fromHICON(hIcon)));
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    Logger::init();
    Logger::info("Naraka Map Tool started."); // 测试输出

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    SetProcessDPIAware();

    ConfigManager::init("config.json");

    QApplication a(argc, argv);
    a.setWindowIcon(loadAppIcon()); // 全局窗口图标（任务栏 / 标题栏 / Alt-Tab）

    // 1. 创建组件
    OverlayWindow overlay;
    overlay.init(GetModuleHandle(NULL));
    overlay.setMap("2", "龙隐洞天");
    overlay.setVisible(false); // 初始隐藏

    ControlPanel panel;
    panel.show();

    MapStatusDetector detector;

    // 2. 建立逻辑连接 (信号与槽)

    // UI -> Overlay (更新地图和资源)
    QObject::connect(&panel, &ControlPanel::mapChanged, [&](const std::string& id, const std::string& name) {
        overlay.setMap(id, name);
        });
    QObject::connect(&panel, &ControlPanel::selectionChanged, [&](const std::vector<std::string>& keys) {
        overlay.updateResources(keys);
        });

    // Detector -> Overlay (控制显示与标记)
    QObject::connect(&detector, &MapStatusDetector::mapVisibilityChanged, [&](bool visible) {
        overlay.setVisible(visible);
        });
    QObject::connect(&detector, &MapStatusDetector::altTriggered, [&]() {
        overlay.handleAltAction();
        });

    QObject::connect(&panel, &ControlPanel::toggleBackground, [&](bool show) {
        overlay.setShowBackground(show);
        });
    return a.exec(); // 开启 Qt 标准事件循环
}