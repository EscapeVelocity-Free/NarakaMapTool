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

signals:
    void mapVisibilityChanged(bool visible); // 地图打开/关闭信号
    void altTriggered();                     // Alt 键触发信号

private slots:
    void processTick(); // 定时器触发的检测函数

private:
    bool isPixelAreaWhite(int radius);

    QTimer* m_timer;
    bool m_isMapOpen = false;
    bool m_altWasPressed = false;

    int m_whiteCount = 0;
    int m_blackCount = 0;
    const int THRESHOLD = 3; // 连续检测阈值
};

#endif