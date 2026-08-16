#include "map_status_detector.h"
#include <cmath>
#include "config_manager.h"
#include "logger.h"

MapStatusDetector::MapStatusDetector(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MapStatusDetector::processTick);
    m_timer->start(30); // 每30ms检测一次
    Logger::info("Map status detector started: interval_ms=30 threshold={} detector=({}, {})",
        THRESHOLD, ConfigManager::detectorX, ConfigManager::detectorY);
}

void MapStatusDetector::processTick() {
    // 1. 检测地图状态
    bool currentWhite = isPixelAreaWhite(2);
    if (currentWhite) {
        m_whiteCount++; m_blackCount = 0;
        if (m_whiteCount >= THRESHOLD && !m_isMapOpen) {
            m_isMapOpen = true;
            Logger::info("Map open detected after {} consecutive white samples.", m_whiteCount);
            emit mapVisibilityChanged(true);
        }
    }
    else {
        m_blackCount++; m_whiteCount = 0;
        if (m_blackCount >= THRESHOLD && m_isMapOpen) {
            m_isMapOpen = false;
            Logger::info("Map close detected after {} consecutive non-white samples.", m_blackCount);
            emit mapVisibilityChanged(false);
        }
    }

    // 2. 检测地图打开时有效的快捷键
    if (m_isMapOpen) {
        if (isKeyPressedOnce(VK_LMENU, m_altWasPressed)) {
            Logger::info("Detected Alt navigation shortcut while map is open.");
            emit altTriggered();
        }

        if (isKeyPressedOnce(VK_SCROLL, m_routeToggleWasPressed)) {
            Logger::info("Detected ScrollLock route visibility shortcut while map is open.");
            emit routeToggleTriggered();
        }
        if (isKeyPressedOnce(VK_PAUSE, m_routeStartWasPressed)) {
            Logger::info("Detected Pause route start shortcut while map is open.");
            emit routeStartTriggered();
        }
        if (isKeyPressedOnce(VK_INSERT, m_routeExcludeWasPressed)) {
            Logger::info("Detected Insert route exclusion shortcut while map is open.");
            emit routeExcludeTriggered();
        }
        if (isKeyPressedOnce(VK_HOME, m_routeResetWasPressed)) {
            Logger::info("Detected Home route reset shortcut while map is open.");
            emit routeResetTriggered();
        }
    }
    else {
        clearRouteKeyStates();
    }
}

bool MapStatusDetector::isKeyPressedOnce(int vk, bool& wasPressed) {
    SHORT state = GetAsyncKeyState(vk);
    bool down = (state & 0x8000) != 0;
    bool pressedSinceLastCall = (state & 0x0001) != 0;
    bool triggered = pressedSinceLastCall || (down && !wasPressed);
    wasPressed = down;
    return triggered;
}

void MapStatusDetector::clearRouteKeyStates() {
    m_altWasPressed = (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
    m_routeToggleWasPressed = (GetAsyncKeyState(VK_SCROLL) & 0x8000) != 0;
    m_routeStartWasPressed = (GetAsyncKeyState(VK_PAUSE) & 0x8000) != 0;
    m_routeExcludeWasPressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    m_routeResetWasPressed = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
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
