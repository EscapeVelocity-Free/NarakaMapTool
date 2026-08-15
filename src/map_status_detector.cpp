#include "map_status_detector.h"
#include <cmath>
#include "config_manager.h"
#include "logger.h"

MapStatusDetector::MapStatusDetector(QObject* parent) : QObject(parent) {
    // 预创建采样缓冲（7x7 足够容纳检测点周边区域），复用避免每次 tick 创建/销毁
    m_sampleDC = CreateCompatibleDC(nullptr);
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = 7;
    bitmapInfo.bmiHeader.biHeight = -7; // top-down
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    m_sampleBitmap = CreateDIBSection(
        m_sampleDC, &bitmapInfo, DIB_RGB_COLORS, &m_sampleBits, nullptr, 0);
    if (m_sampleBitmap) {
        m_samplePreviousBitmap = SelectObject(m_sampleDC, m_sampleBitmap);
    }
    else {
        Logger::error("Failed to create map detector sample buffer.");
    }

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MapStatusDetector::processTick);
    m_timer->start(30); // 每30ms检测一次
    Logger::info("Map status detector started: interval_ms=30 threshold={} detector=({}, {})",
        THRESHOLD, ConfigManager::detectorX, ConfigManager::detectorY);
}

MapStatusDetector::~MapStatusDetector() {
    if (m_sampleDC && m_samplePreviousBitmap) {
        SelectObject(m_sampleDC, m_samplePreviousBitmap);
    }
    if (m_sampleBitmap) {
        DeleteObject(m_sampleBitmap);
    }
    if (m_sampleDC) {
        DeleteDC(m_sampleDC);
    }
    m_sampleDC = nullptr;
    m_sampleBitmap = nullptr;
    m_samplePreviousBitmap = nullptr;
    m_sampleBits = nullptr;
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
    // 性能优化：一次 BitBlt 把检测点周边 (2*radius+1)^2 区域拷入内存 DIB，
    // 再从内存读取像素，替代 25 次跨 GDI 的 GetPixel 调用。
    if (!m_sampleDC || !m_sampleBitmap) {
        return false;
    }

    const int size = radius * 2 + 1;
    const int originX = ConfigManager::detectorX - radius;
    const int originY = ConfigManager::detectorY - radius;

    HDC screenDC = GetDC(NULL);
    if (!screenDC) {
        return false;
    }
    const BOOL copied = BitBlt(m_sampleDC, 0, 0, size, size, screenDC, originX, originY, SRCCOPY);
    ReleaseDC(NULL, screenDC);
    if (!copied || !m_sampleBits) {
        return false;
    }

    const auto* pixels = static_cast<const unsigned char*>(m_sampleBits);
    for (int dy = 0; dy < size; ++dy) {
        for (int dx = 0; dx < size; ++dx) {
            // 圆内采样（与原逻辑一致，略去超出圆半径的角点）
            const int relX = dx - radius;
            const int relY = dy - radius;
            if (std::sqrt(static_cast<double>(relX * relX + relY * relY)) > radius) {
                continue;
            }
            const unsigned char* p = pixels + (dy * size + dx) * 4;
            if (p[2] != 255 || p[1] != 255 || p[0] != 255) { // BGRA 布局
                return false;
            }
        }
    }
    return true;
}
