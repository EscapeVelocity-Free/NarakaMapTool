#ifndef MAP_STATUS_DETECTOR_H
#define MAP_STATUS_DETECTOR_H

#include <QObject>
#include <QTimer>
#include <vector>
#include <windows.h>

class MapStatusDetector : public QObject {
    Q_OBJECT
public:
    explicit MapStatusDetector(QObject* parent = nullptr);
    void setShortcutHandlingSuspended(bool suspended);

signals:
    void mapVisibilityChanged(bool visible); // 地图打开/关闭信号
    void altTriggered();                     // Alt 键触发信号
    void routeToggleTriggered();             // ScrollLock: show/hide marker route
    void routeStartTriggered();              // Pause: use nearest marker as route start
    void routeExcludeTriggered();            // Insert: exclude/restore nearest marker
    void routeResetTriggered();              // Home: reset route state

private slots:
    void processTick(); // 定时器触发的检测函数

private:
    bool isPixelAreaWhite(int radius);
    bool isKeyPressedOnce(int vk, bool& wasPressed);
    void clearRouteKeyStates();

    QTimer* m_timer;
    bool m_isMapOpen = false;
    bool m_altWasPressed = false;
    bool m_routeToggleWasPressed = false;
    bool m_routeStartWasPressed = false;
    bool m_routeExcludeWasPressed = false;
    bool m_routeResetWasPressed = false;
    bool m_shortcutHandlingSuspended = false;

    int m_whiteCount = 0;
    int m_blackCount = 0;
    const int THRESHOLD = 3; // 连续检测阈值
};

#endif
