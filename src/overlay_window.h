#ifndef OVERLAY_WINDOW_H
#define OVERLAY_WINDOW_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <future>
#include "nlohmann/json.hpp"
#include "map_transform.h"

// 定义地图在屏幕上的物理位置常量
const int MAP_UI_X = 1413;
const int MAP_UI_Y = 149;
const int MAP_UI_SIZE = 1093;

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    void init(HINSTANCE hInst);
    void setMap(const std::string& mapId, const std::string& mapName);
    void setMapLayer(int layer);
    void updateResources(const std::vector<std::string>& keys);
    void handleAltAction();
    void handleMouseWheel(int wheelDelta, int screenX, int screenY, bool injected);
    void toggleRouteVisible();
    void resetRoute();
    void setNearestPointAsRouteStart();
    void toggleNearestPointExcluded();
    void setVisible(bool visible); // 控制窗口显示隐藏
    bool isVisible();              // 获取当前状态
    void setShowBackground(bool show);
    void setBackgroundOpacity(int opacityPercent);
    void setMapZoomEnabled(bool enabled);
    void setAlwaysVisible(bool enabled);
private:
    struct Point;

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void loadData();
    void loadMapBackground();
    Gdiplus::Image* GetIcon(std::string type);
    Gdiplus::Image* GetZoneImage(const std::string& zoneId);
    void rebuildRoute();
    void pollRouteResult();
    void restoreZoomQuality();
    void drawRoute(Gdiplus::Graphics& g);
    void drawRouteMarkerState(Gdiplus::Graphics& g, const Point& pt, int localX, int localY);
    MapScreenPoint pointToScreen(const Point& point) const;
    int findNearestPointToCursor(double maxDistance) const;
    bool ensureRenderSurface();
    bool renderBackgroundFrame();
    void releaseRenderSurface();
    void updateVisibility();
    void invalidate(bool contentDirty = true);

    HWND m_hwnd;
    ULONG_PTR m_gdiToken;

    std::string m_currentMapId;
    std::string m_currentMapName; // 当前地图名称
    int m_currentLayer = 0;
    std::vector<std::string> m_activeKeys;

    struct Point {
        std::string id;
        std::string type;
        double mapX; // 2048 地图坐标系中的屏幕方向 X
        double mapY; // 2048 地图坐标系中的屏幕方向 Y
        double radiusMap = 0.0; // 范围圆半径（地图坐标），0 表示不绘制
    };
    struct Zone {
        std::string id;      // 稳定ID（区域图片名）
        std::string type;    // "highResourceZone"
        double mapX;         // 区域中心地图坐标 X
        double mapY;         // 区域中心地图坐标 Y
        double halfWMap;     // 区域半宽（地图坐标）
        double halfHMap;     // 区域半高（地图坐标）
    };
    std::vector<Point> m_points;
    std::vector<Zone> m_zones;
    std::vector<size_t> m_routeOrder;
    std::set<std::string> m_excludedPointIds;
    std::string m_routeStartId;
    // 路线后台计算：future 持有计算任务，m_routeRevision 用于丢弃过期结果
    std::future<std::pair<uint64_t, std::vector<size_t>>> m_routeFuture;
    uint64_t m_routeRevision = 0;
    std::map<std::string, Gdiplus::Image*> m_iconCache;
    std::map<std::string, Gdiplus::Image*> m_zoneImageCache;
    Gdiplus::Image* m_bgImg;

    int m_winX;
    int m_winY;
    int m_winSize;
    HDC m_renderDC = nullptr;
    HBITMAP m_renderBitmap = nullptr;
    HGDIOBJ m_renderPreviousBitmap = nullptr;
    void* m_renderBits = nullptr;
    HDC m_backgroundDC = nullptr;
    HBITMAP m_backgroundBitmap = nullptr;
    HGDIOBJ m_backgroundPreviousBitmap = nullptr;
    void* m_backgroundBits = nullptr;
    HDC m_overlayDC = nullptr;
    HBITMAP m_overlayBitmap = nullptr;
    HGDIOBJ m_overlayPreviousBitmap = nullptr;
    void* m_overlayBits = nullptr;
    MapTransform m_mapTransform;
    bool m_showBackground = false; // 默认不显示
    int m_backgroundOpacity = 100;
    bool m_backgroundFrameDirty = true;
    bool m_overlayFrameDirty = true;
    bool m_mapZoomEnabled = false; // 默认不允许手动缩放
    bool m_mapDetectedVisible = false; // 默认等待游戏地图检测结果
    bool m_alwaysVisible = false; // 默认跟随游戏地图显示状态
    bool m_showRoute = false;
    int m_wheelRemainder = 0;
    bool m_framePending = false;
    // 缩放流畅度优化：滚动期间用低质量插值，静止一段时间后切回高质量重绘
    bool m_lowQualityZoom = false;
    ULONGLONG m_lastWheelTick = 0;
};

#endif
