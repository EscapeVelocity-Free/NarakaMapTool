#include "overlay_window.h"
#include <fstream>
#include <shlwapi.h>
#include <cmath>
#include "config_manager.h"
#include "logger.h"

#pragma comment(lib, "Shlwapi.lib")
using namespace Gdiplus;
namespace fs = std::filesystem;

// 图标名映射
std::string GetMappingFileName(std::string type) {
    static std::map<std::string, std::string> nameMap = {
        {"goldenToad", "toad"}, {"questSerialCache", "quest"}, {"pear", "sand_pear"},{"wishingWell", "wishing_well"},
        {"miniShrine", "mini_shrine"}, {"flyingTarget", "flying_target"}, {"fireflyCage", "firefly_cage"},
        {"strongPoint", "strong_point"}, {"questBell", "quest_bell"}, {"questCache", "quest_cache"},
        {"questShrine", "quest_shrine"}, {"reverseBounty", "reverse_bounty"}, {"treasureCoin", "treasure_coin"},
        {"pricklyPear", "prickly_pear"}, {"soaringArm", "soaring_arm"}, {"bearTrap", "bear_trap"},
        {"soulAltar", "soul_altar"}, {"treasureCave", "treasure_cave"}, {"forbiddenSeal", "forbidden_seal"},
        {"gateOfYang", "gate_of_yang"}, {"carnivorousYam", "carnivorous_yam"}, {"snowLotus", "snow_lotus"},
        {"springSource", "spring_source"}, {"riftDealer", "rift_dealer"}, {"weaponRack", "weapon_rack"},
        {"healingTree", "healing_tree"}, {"prayerShrine", "prayer_shrine"}, {"floatingPile", "floating_pile"}
    };
    if (nameMap.count(type)) return nameMap[type];
    return type;
}

OverlayWindow::OverlayWindow() : m_hwnd(NULL), m_bgImg(NULL), m_currentMapId("2"), m_currentMapName(u8"龙隐洞天") {
    GdiplusStartupInput si;
    GdiplusStartup(&m_gdiToken, &si, NULL);
}

OverlayWindow::~OverlayWindow() {
    if (m_bgImg) delete m_bgImg;
    for (auto& pair : m_iconCache) delete pair.second;
    GdiplusShutdown(m_gdiToken);
}

void OverlayWindow::setMap(const std::string& mapId, const std::string& mapName) {
    m_currentMapId = mapId;
    m_currentMapName = mapName;

    if (m_bgImg) { 
        delete m_bgImg; 
        m_bgImg = nullptr; 
    }
    
    // 1. Map Chinese name to English filename
    std::string engBaseName = "unknown";
    if (mapName == "聚窟州" || mapName == u8"聚窟州") {
        engBaseName = "morus";
    }
    else if (mapName == "火罗国" || mapName == u8"火罗国") {
        engBaseName = "holoroth";
    }
    else if (mapName == "龙隐洞天" || mapName == u8"龙隐洞天") {
        engBaseName = "dragon";
    }

    try {
        // 2. Construct the new path (e.g., morus_3.png)
        namespace fs = std::filesystem;
        std::string fileName = engBaseName + "_3.png";
        fs::path fullPath = fs::u8path(ConfigManager::mapImagePath) / fileName;

        Logger::debug("Loading map file: {}", fullPath.string());

        if (!fs::exists(fullPath)) {
            Logger::error("Target map file does not exist: {}", fullPath.string());
        }
        else {
            // 3. Load the image
            m_bgImg = Image::FromFile(fullPath.wstring().c_str());

            if (m_bgImg->GetLastStatus() != Ok) {
                Logger::error("GDI+ failed to load image. Status: {}", (int)m_bgImg->GetLastStatus());
                delete m_bgImg;
                m_bgImg = nullptr;
            }
            else {
                Logger::info("Map switched successfully to: {}", engBaseName);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::error("Critical error during map switching: {}", e.what());
    }


    loadData();
    if (m_hwnd) InvalidateRect(m_hwnd, NULL, TRUE);
}

void OverlayWindow::init(HINSTANCE hInst) {
    // ��̬���㴰�ڼ�������
    m_winX = ConfigManager::mapOffsetX;
    m_winY = ConfigManager::mapOffsetY;
    m_winSize = ConfigManager::mapUiSize;

    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInst;
    wcex.lpszClassName = L"NarakaOverlayWindowClass";
    wcex.hbrBackground = CreateSolidBrush(RGB(1, 1, 1)); // ����ˢΪ͸����ɫ
    RegisterClassExW(&wcex);

    m_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wcex.lpszClassName, L"NarakaMap", WS_POPUP, m_winX, m_winY, m_winSize, m_winSize, NULL, NULL, hInst, this);

    SetLayeredWindowAttributes(m_hwnd, RGB(1, 1, 1), 0, LWA_COLORKEY);
    ShowWindow(m_hwnd, SW_SHOW);
}

void OverlayWindow::updateResources(const std::vector<std::string>& keys) {
    m_activeKeys = keys;
    loadData();
    if (m_hwnd) InvalidateRect(m_hwnd, NULL, TRUE);
}

void OverlayWindow::loadData() {
    std::ifstream f(std::filesystem::path(ConfigManager::resourcePath) / "resources_naraka.json");
    if (!f.is_open()) return;
    try {
        nlohmann::json data; f >> data;
        m_points.clear();
        for (auto& item : data) {
            if (item.value("map", "") == m_currentMapId) {
                for (const auto& key : m_activeKeys) {
                    if (item.contains(key)) {
                        for (auto& marker : item[key]["MarkerList"]) {
                            double gx = 0, gy = 0;
                            auto& pos = marker["pos"];
                            if (pos[0].is_string()) gx = std::stod(pos[0].get<std::string>());
                            else gx = pos[0].get<double>();
                            if (pos[1].is_string()) gy = std::stod(pos[1].get<std::string>());
                            else gy = pos[1].get<double>();

                            // 存储绝对坐标
                            int ax = (int)((gx / 2048.0) * m_winSize) + m_winX;
                            int ay = (int)(((2048.0 - gy) / 2048.0) * m_winSize) + m_winY;
                            m_points.push_back({ key, ax, ay });
                        }
                    }
                }
            }
        }
    }
    catch (...) {
        Logger::error("loadData error");
    }
}

// 模拟鼠标滚轮
void Win32_MouseScroll(bool up, int amount, int times) {
    for (int i = 0; i < times; ++i) {
        Sleep(5); 
        int wheel_movement = up ? (120 * amount) : (-120 * amount);
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)wheel_movement, 0);
    }
    Sleep(60);
}

// 模拟鼠标点击
void Win32_MouseClick(int x, int y) {
    SetCursorPos(x, y);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

// 模拟键盘按键
void Win32_KeyPress(BYTE vkCode) {
    keybd_event(vkCode, 0, 0, 0); // 按下
    Sleep(20);
    keybd_event(vkCode, 0, KEYEVENTF_KEYUP, 0); // 弹起
}

void OverlayWindow::handleAltAction() {
    POINT cur;
    GetCursorPos(&cur);

    // 1. 判断是否在地图 UI 范围内
    if (cur.x >= m_winX && cur.x <= m_winX + m_winSize &&
        cur.y >= m_winY && cur.y <= m_winY + m_winSize) {

        double min_dist = 99999.0;
        Point* target = nullptr;

        // 2. 寻找最近的资源点
        for (auto& pt : m_points) {
            double d = std::sqrt(std::pow(pt.absX - cur.x, 2) + std::pow(pt.absY - cur.y, 2));
            if (d < min_dist) {
                min_dist = d;
                target = &pt;
            }
        }

        // 3. 执行：自动寻路资源 (阈值 100 像素)
        if (target && min_dist < 100.0) {
            Logger::info("Starting auto-navigation to resource: {}", target->type);

            // Step 1: 模拟滚轮向上滚动 50 次 (times=50)
            SetCursorPos(target->absX, target->absY);
            Win32_MouseScroll(true, 1, 50);

            // Step 2: 在目标点双击
            Win32_MouseClick(target->absX, target->absY);

            // Step 3: 等待 100ms
            Sleep(50);

            // Step 4: 点击确认按钮 (1358, 1207)
            // 这里是根据 Python 脚本中 const_var.g_地图是否展开判定点 坐标来确定的
            Win32_MouseClick(ConfigManager::detectorX, ConfigManager::detectorY);
            
            // Step 5: 等待 100ms
            Sleep(50);

            // Step 6: 点击 ESC 键 (VK_ESCAPE)
            Win32_KeyPress(VK_ESCAPE);

            Logger::info("Resource point execution completed.");
        }
    }
}

Gdiplus::Image* OverlayWindow::GetIcon(std::string type) {
    // 1. Check if the icon is already in cache to save CPU/IO
    if (m_iconCache.count(type)) {
        return m_iconCache[type];
    }

    // 2. Get the actual filename (e.g., "goldenToad" -> "toad")
    std::string fileName = GetMappingFileName(type);

    try {
        // 3. Construct the absolute path
        // ConfigManager::resourcePath should be the directory containing "icons" folder
        namespace fs = std::filesystem;
        fs::path iconPath = fs::u8path(ConfigManager::resourcePath) / "icons" / (fileName + ".png");

        // 4. Load the image using Wide String path (essential for Chinese characters)
        Gdiplus::Image* img = new Gdiplus::Image(iconPath.wstring().c_str());

        // 5. Check if the image loaded successfully
        if (img->GetLastStatus() != Gdiplus::Ok) {
            Logger::warn("Failed to load icon: {} Status: {}", iconPath.string(), (int)img->GetLastStatus());
            delete img;
            m_iconCache[type] = nullptr; // Cache the nullptr to avoid re-searching
            return nullptr;
        }

        // 6. Success: Store in cache and return
        m_iconCache[type] = img;
        return img;

    }
    catch (const std::exception& e) {
        Logger::error("Error in GetIcon for {}: {}", type, e.what());
        return nullptr;
    }
}

void OverlayWindow::paint(HDC hdc) {
    // 传统的双缓冲逻辑
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, m_winSize, m_winSize);
    SelectObject(memDC, memBitmap);

    Graphics g(memDC);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // 1. 填充底色为透明键色
    SolidBrush transBrush(Color(255, 1, 1, 1));
    g.FillRectangle(&transBrush, 0, 0, m_winSize, m_winSize);

    // 2. 根据开关决定是否绘制地图
    if (m_showBackground && m_bgImg && m_bgImg->GetLastStatus() == Ok) {
        g.DrawImage(m_bgImg, 0, 0, m_winSize, m_winSize);
    }

    // 3. 绘制图标 (永远显示)
    const int drawSize = 24;
    for (const auto& pt : m_points) {
        Image* icon = GetIcon(pt.type);
        int localX = pt.absX - m_winX;
        int localY = pt.absY - m_winY;
        if (icon) {
            g.DrawImage(icon, localX - drawSize / 2, localY - drawSize / 2, drawSize, drawSize);
        }
        else {
            SolidBrush red(Color::Red);
            g.FillEllipse(&red, localX - 3, localY - 3, 6, 6);
        }
    }

    // 4. 绘制红边框
    Pen redPen(Color::Red, 4);
    g.DrawRectangle(&redPen, 1, 1, m_winSize - 2, m_winSize - 2);

    // 贴图到屏幕
    BitBlt(hdc, 0, 0, m_winSize, m_winSize, memDC, 0, 0, SRCCOPY);

    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    OverlayWindow* obj = (msg == WM_NCCREATE) ? (OverlayWindow*)((LPCREATESTRUCT)lp)->lpCreateParams : (OverlayWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)obj);
    if (msg == WM_PAINT && obj) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
        obj->paint(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_DESTROY) return 0;
    return DefWindowProc(hWnd, msg, wp, lp);
}

void OverlayWindow::setVisible(bool visible) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

bool OverlayWindow::isVisible() {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void OverlayWindow::setShowBackground(bool show) {
    m_showBackground = show;
    if (m_hwnd) InvalidateRect(m_hwnd, NULL, TRUE);
}