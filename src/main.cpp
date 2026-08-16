#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QTimer>
#include <QtPlugin>
#include "control_panel.h"
#include "overlay_window.h"
#include "map_status_detector.h"
#include "mouse_input_monitor.h"
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
    Logger::info("Process startup arguments received: argc={}", argc);

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    SetProcessDPIAware();
    Logger::info("High-DPI scaling and process DPI awareness enabled.");

    ConfigManager::init("config.json");
    Logger::info("Configuration initialization completed.");

    QApplication a(argc, argv);
    a.setWindowIcon(loadAppIcon()); // 全局窗口图标（任务栏 / 标题栏 / Alt-Tab）
    Logger::info("Qt application created and application icon assigned.");

    // 1. 创建组件
    OverlayWindow overlay;
    overlay.init(GetModuleHandle(NULL));
    overlay.setMap("2", "龙隐洞天");
    overlay.setVisible(false); // 初始隐藏
    Logger::info("Overlay initialized with default map id=2 name=龙隐洞天 and hidden state.");

    ControlPanel panel;
    panel.show();
    Logger::info("Control panel created and shown.");
    QTimer::singleShot(0, &panel, [&panel]() {
        panel.showNormal();
        panel.raise();
        panel.activateWindow();
        Logger::debug("Control panel activated after the initial event loop turn.");
        });

    MapStatusDetector detector;
    Logger::info("Map status detector created.");

    MouseInputMonitor mouseInputMonitor;

    // 2. 建立逻辑连接 (信号与槽)

    // UI -> Overlay (更新地图和资源)
    QObject::connect(&panel, &ControlPanel::mapChanged, [&](const std::string& id, const std::string& name) {
        Logger::info("Main bridge received map change: id={} name={}", id, name);
        overlay.setMap(id, name);
        });
    QObject::connect(&panel, &ControlPanel::layerChanged, [&](int layer) {
        Logger::info("Main bridge received map layer change: layer={}", layer);
        overlay.setMapLayer(layer);
        });
    QObject::connect(&panel, &ControlPanel::selectionChanged, [&](const std::vector<std::string>& keys) {
        Logger::info("Main bridge received resource selection update: key_count={}", keys.size());
        overlay.updateResources(keys);
        });

    // Detector -> Overlay (控制显示与标记)
    QObject::connect(&detector, &MapStatusDetector::mapVisibilityChanged, [&](bool visible) {
        Logger::info("Main bridge received map visibility update: visible={}", visible);
        overlay.setVisible(visible);
        });
    QObject::connect(&detector, &MapStatusDetector::altTriggered, [&]() {
        Logger::info("Main bridge received Alt navigation trigger.");
        overlay.handleAltAction();
        });
    QObject::connect(&detector, &MapStatusDetector::routeToggleTriggered, [&]() {
        Logger::info("Main bridge received route visibility trigger.");
        overlay.toggleRouteVisible();
        });
    QObject::connect(&detector, &MapStatusDetector::routeStartTriggered, [&]() {
        Logger::info("Main bridge received route start trigger.");
        overlay.setNearestPointAsRouteStart();
        });
    QObject::connect(&detector, &MapStatusDetector::routeExcludeTriggered, [&]() {
        Logger::info("Main bridge received route exclude trigger.");
        overlay.toggleNearestPointExcluded();
        });
    QObject::connect(&detector, &MapStatusDetector::routeResetTriggered, [&]() {
        Logger::info("Main bridge received route reset trigger.");
        overlay.resetRoute();
        });

    QObject::connect(&mouseInputMonitor, &MouseInputMonitor::wheelChanged,
        [&](int wheelDelta, int screenX, int screenY, bool injected) {
            overlay.handleMouseWheel(wheelDelta, screenX, screenY, injected);
        });

    QObject::connect(&panel, &ControlPanel::toggleBackground, [&](bool show) {
        Logger::info("Main bridge received background visibility update: show={}", show);
        overlay.setShowBackground(show);
        });
    QObject::connect(&panel, &ControlPanel::backgroundOpacityChanged, [&](int opacityPercent) {
        Logger::info("Main bridge received background opacity update: percent={}", opacityPercent);
        overlay.setBackgroundOpacity(opacityPercent);
        });
    QObject::connect(&panel, &ControlPanel::toggleMapZoom, [&](bool enabled) {
        Logger::info("Main bridge received map zoom update: enabled={}", enabled);
        overlay.setMapZoomEnabled(enabled);
        });
    QObject::connect(&panel, &ControlPanel::toggleAlwaysVisible, [&](bool enabled) {
        Logger::info("Main bridge received always-visible update: enabled={}", enabled);
        overlay.setAlwaysVisible(enabled);
    });
    QObject::connect(&panel, &ControlPanel::toggleBorder, [&](bool enabled) {
        Logger::info("Main bridge received border visibility update: enabled={}", enabled);
        overlay.setShowBorder(enabled);
    });
    mouseInputMonitor.start();
    Logger::info("All UI, detector, and overlay signal connections established. Entering event loop.");
    return a.exec(); // 开启 Qt 标准事件循环
}
