#ifndef OVERLAY_WINDOW_H
#define OVERLAY_WINDOW_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "nlohmann/json.hpp"

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
    void toggleRouteVisible();
    void resetRoute();
    void setNearestPointAsRouteStart();
    void toggleNearestPointExcluded();
    void setVisible(bool visible); // 控制窗口显示隐藏
    bool isVisible();              // 获取当前状态
    void setShowBackground(bool show);
private:
    struct Point;

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void loadData();
    void loadMapBackground();
    Gdiplus::Image* GetIcon(std::string type);
    Gdiplus::Image* GetZoneImage(const std::string& zoneId);
    void rebuildRoute();
    void drawRoute(Gdiplus::Graphics& g);
    void drawRouteMarkerState(Gdiplus::Graphics& g, const Point& pt, int localX, int localY);
    int findNearestPointToCursor(double maxDistance) const;
    void invalidate();

    HWND m_hwnd;
    ULONG_PTR m_gdiToken;

    std::string m_currentMapId;
    std::string m_currentMapName; // 当前地图名称
    int m_currentLayer = 0;
    std::vector<std::string> m_activeKeys;

    struct Point {
        std::string id;
        std::string type;
        int absX; // 屏幕绝对像素X
        int absY; // 屏幕绝对像素Y
        int radius = 0; // 范围圆半径（屏幕像素），0 表示不绘制
    };
    struct Zone {
        std::string id;      // 稳定ID（区域图片名）
        std::string type;    // "highResourceZone"
        int absX;            // 区域中心屏幕绝对像素X
        int absY;            // 区域中心屏幕绝对像素Y
        int absHalfW;        // 区域半宽（屏幕像素）
        int absHalfH;        // 区域半高（屏幕像素）
    };
    std::vector<Point> m_points;
    std::vector<Zone> m_zones;
    std::vector<size_t> m_routeOrder;
    std::set<std::string> m_excludedPointIds;
    std::string m_routeStartId;
    std::map<std::string, Gdiplus::Image*> m_iconCache;
    std::map<std::string, Gdiplus::Image*> m_zoneImageCache;
    Gdiplus::Image* m_bgImg;

    int m_winX;
    int m_winY;
    int m_winSize;
    bool m_showBackground = false; // 默认不显示
    bool m_showRoute = false;
};

#endif
