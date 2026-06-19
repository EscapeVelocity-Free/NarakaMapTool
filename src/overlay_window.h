#ifndef OVERLAY_WINDOW_H
#define OVERLAY_WINDOW_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>
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
    void updateResources(const std::vector<std::string>& keys);
    void handleAltAction();
    void setVisible(bool visible); // 控制窗口显示隐藏
    bool isVisible();              // 获取当前状态
    void setShowBackground(bool show);
private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void loadData();
    Gdiplus::Image* GetIcon(std::string type);

    HWND m_hwnd;
    ULONG_PTR m_gdiToken;

    std::string m_currentMapId;
    std::string m_currentMapName; // 修复：添加此声明
    std::vector<std::string> m_activeKeys;

    struct Point {
        std::string type;
        int absX; // 屏幕绝对像素X
        int absY; // 屏幕绝对像素Y
    };
    std::vector<Point> m_points;
    std::map<std::string, Gdiplus::Image*> m_iconCache;
    Gdiplus::Image* m_bgImg;

    int m_winX;
    int m_winY;
    int m_winSize;
    bool m_showBackground = false; // 默认为显示
};

#endif