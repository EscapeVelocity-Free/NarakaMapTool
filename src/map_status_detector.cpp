#include "map_status_detector.h"
#include <cmath>
#include "config_manager.h"

MapStatusDetector::MapStatusDetector(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MapStatusDetector::processTick);
    m_timer->start(30); // 每30ms检测一次
}

void MapStatusDetector::processTick() {
    // 1. 检测地图状态
    bool currentWhite = isPixelAreaWhite(2);
    if (currentWhite) {
        m_whiteCount++; m_blackCount = 0;
        if (m_whiteCount >= THRESHOLD && !m_isMapOpen) {
            m_isMapOpen = true;
            emit mapVisibilityChanged(true);
        }
    }
    else {
        m_blackCount++; m_whiteCount = 0;
        if (m_blackCount >= THRESHOLD && m_isMapOpen) {
            m_isMapOpen = false;
            emit mapVisibilityChanged(false);
        }
    }

    // 2. 检测 Alt 按键 (仅在地图打开时有效)
    if (m_isMapOpen) {
        bool altDown = (GetAsyncKeyState(VK_LMENU) & 0x8000);
        if (altDown && !m_altWasPressed) {
            emit altTriggered();
        }
        m_altWasPressed = altDown;
    }
}

bool MapStatusDetector::isPixelAreaWhite(int radius) {
    HDC hdc = GetDC(NULL);
    bool allWhite = true;

    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            if (std::sqrt(dx * dx + dy * dy) <= radius) {
                COLORREF color = GetPixel(hdc, ConfigManager::detectorX + dx, ConfigManager::detectorY + dy);
                if (GetRValue(color) != 255 || GetGValue(color) != 255 || GetBValue(color) != 255) {
                    allWhite = false;
                    break;
                }
            }
        }
        if (!allWhite) break;
    }
    ReleaseDC(NULL, hdc);
    return allWhite;
}