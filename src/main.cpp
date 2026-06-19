#include <QApplication>
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

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    Logger::init();
    Logger::info("Naraka Map Tool started."); // 测试输出

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    SetProcessDPIAware();

    ConfigManager::init("config.json");

    QApplication a(argc, argv);

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