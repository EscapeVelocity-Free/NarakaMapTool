#include "overlay_window.h"
#include <fstream>
#include <filesystem>
#include <shlwapi.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <limits>
#include <sstream>
#include <iterator>
#include <utility>
#include "config_manager.h"
#include "logger.h"

#pragma comment(lib, "Shlwapi.lib")
using namespace Gdiplus;
namespace fs = std::filesystem;

namespace {
constexpr double ROUTE_POINT_HIT_RADIUS = 80.0;
constexpr int ROUTE_OPTIMIZE_BUDGET_MS = 35;
using ScreenCoord = std::pair<int, int>;

double ScreenDistanceSq(const ScreenCoord& a, const ScreenCoord& b) {
    double dx = static_cast<double>(a.first - b.first);
    double dy = static_cast<double>(a.second - b.second);
    return dx * dx + dy * dy;
}

double RouteEdgeLength(const std::vector<ScreenCoord>& coords, const std::vector<size_t>& order, size_t left, size_t right) {
    return std::sqrt(ScreenDistanceSq(coords[order[left]], coords[order[right]]));
}

std::vector<size_t> BuildNearestNeighborRoute(const std::vector<ScreenCoord>& coords, const std::vector<size_t>& candidates, size_t startPointIndex) {
    if (candidates.empty()) return {};

    std::vector<size_t> route;
    route.reserve(candidates.size());

    std::vector<bool> visited(coords.size(), false);
    size_t current = startPointIndex;
    if (std::find(candidates.begin(), candidates.end(), current) == candidates.end()) {
        current = candidates.front();
    }

    route.push_back(current);
    visited[current] = true;

    while (route.size() < candidates.size()) {
        size_t bestIndex = candidates.front();
        double bestDist = (std::numeric_limits<double>::max)();

        for (size_t candidate : candidates) {
            if (visited[candidate]) continue;
            double dist = ScreenDistanceSq(coords[current], coords[candidate]);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = candidate;
            }
        }

        current = bestIndex;
        visited[current] = true;
        route.push_back(current);
    }

    return route;
}

void OptimizeRoute2Opt(const std::vector<ScreenCoord>& coords, std::vector<size_t>& route, int budgetMs) {
    if (route.size() < 4) return;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
    bool improved = true;

    while (improved && std::chrono::steady_clock::now() < deadline) {
        improved = false;
        for (size_t i = 1; i + 2 < route.size(); ++i) {
            if (std::chrono::steady_clock::now() >= deadline) return;

            for (size_t j = i + 1; j + 1 < route.size(); ++j) {
                double oldLength = RouteEdgeLength(coords, route, i - 1, i) + RouteEdgeLength(coords, route, j, j + 1);
                double newLength = RouteEdgeLength(coords, route, i - 1, j) + RouteEdgeLength(coords, route, i, j + 1);
                if (newLength + 0.001 < oldLength) {
                    std::reverse(route.begin() + i, route.begin() + j + 1);
                    improved = true;
                    break;
                }
            }

            if (improved) break;
        }
    }
}
}

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
        {"healingTree", "healing_tree"}, {"prayerShrine", "prayer_shrine"}, {"floatingPile", "floating_pile"},
        {"riftChestEpic", "rift_chest_epic"}, {"riftChestEpicGuaranteed", "rift_chest_epic_2"},
        {"riftChestLegendary", "rift_chest_legendary"}, {"riftChestEpicSand", "rift_chest_sand"},
        {"riftChestEpicSnow", "rift_chest_snow"}, {"riftChestEpicThunder", "rift_chest_epic_thunder"},
        {"riftChestLegendaryThunder", "rift_chest_legendary_thunder"}, {"riftAirCurrent", "rift_air_current"},
        {"riftBigCollectionContainer", "rift_big_collection_container"}, {"riftBigHamper", "rift_big_hamper"},
        {"riftBigUtilityCabinet", "rift_big_utility_cabinet"}, {"riftBlackFlameSoldier", "rift_black_flame_soldier"},
        {"riftCaveKey", "rift_cave_key"}, {"riftDice", "rift_dice"}, {"riftGateOfYang", "rift_gate_of_yang"},
        {"riftGoldRelic", "rift_gold_relic"}, {"riftLargeCollectionCabinet", "rift_large_collection_cabinet"},
        {"riftLargeCollectionShop", "rift_large_collection_shop"}, {"riftLargeMedicineCabinet", "rift_large_medicine_cabinet"},
        {"riftLetter", "rift_letter"}, {"riftMinecart", "rift_minecart"}, {"riftPiggyBank", "rift_piggy_bank"},
        {"riftQuestGuard", "rift_quest_guard"}, {"riftSecret", "rift_secret"}, {"riftSedanChair", "rift_sedan_chair"},
        {"riftShovel", "rift_shovel"}, {"riftSmallHamper", "rift_small_hamper"},
        {"riftSmallMakeupBox", "rift_small_makeup_box"}, {"riftSteleDecipher", "rift_stele_decipher"},
        {"riftStronghold", "rift_stronghold"}, {"riftStrongholdBoss", "rift_stronghold_boss"},
        {"riftStrongholdMiniBoss", "rift_stronghold_mini_boss"}, {"riftWaterWell", "rift_water_well"},
        {"riftWeaponBox", "rift_weapon_box"}, {"helanArt", "helan_art"},
        {"earthShrine", "prayer_shrine"}, {"ironUrchin", "iron_urchin"},
        {"ironbackTurtle", "ironback_turtle"}, {"spiritClam", "spirit_clam"}
    };
    if (nameMap.count(type)) return nameMap[type];
    return type;
}

static std::string ResolveMapBackgroundName(const std::string& mapId) {
    if (mapId == "0") return "morus_3.png";
    if (mapId == "1") return "holoroth_3.png";
    if (mapId == "2") return "dragon_3.png";
    if (mapId == "3") return "fqhl.png";
    if (mapId == "4") return "rivers_runs_red.png";
    if (mapId == "5") return "wanchu.png";
    return {};
}

OverlayWindow::OverlayWindow() : m_hwnd(NULL), m_bgImg(NULL), m_currentMapId("2"), m_currentMapName(u8"龙隐洞天") {
    GdiplusStartupInput si;
    const Status status = GdiplusStartup(&m_gdiToken, &si, NULL);
    if (status == Ok) {
        Logger::info("GDI+ initialized successfully.");
    }
    else {
        Logger::error("GDI+ initialization failed: status={}", static_cast<int>(status));
    }
}

OverlayWindow::~OverlayWindow() {
    Logger::info("Destroying overlay window: cached_icons={} active_points={}", m_iconCache.size(), m_points.size());
    if (m_bgImg) delete m_bgImg;
    for (auto& pair : m_iconCache) delete pair.second;
    GdiplusShutdown(m_gdiToken);
}

void OverlayWindow::setMap(const std::string& mapId, const std::string& mapName) {
    Logger::info("Overlay map switch requested: previous_id={} target_id={} target_name={} background_enabled={}",
        m_currentMapId, mapId, mapName, m_showBackground);
    m_currentMapId = mapId;
    m_currentMapName = mapName;
    m_routeStartId.clear();
    m_excludedPointIds.clear();
    m_routeOrder.clear();

    if (m_bgImg) { 
        delete m_bgImg; 
        m_bgImg = nullptr; 
    }
    
    try {
        std::string fileName = ResolveMapBackgroundName(mapId);
        if (fileName.empty()) {
            Logger::warn("Unknown map selection: id={} name={}", mapId, mapName);
        }

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
                Logger::info("Map switched successfully: {} ({})", mapId, mapName);
                Logger::info("Map background loaded: id={} name={} file={} dimensions={}x{}",
                    mapId, mapName, fullPath.string(), m_bgImg->GetWidth(), m_bgImg->GetHeight());
            }
        }
    }
    catch (const std::exception& e) {
        Logger::error("Critical error during map switching: {}", e.what());
    }


    loadData();
    invalidate();
}

void OverlayWindow::init(HINSTANCE hInst) {
    // 动态计算窗口几何参数
    m_winX = ConfigManager::mapOffsetX;
    m_winY = ConfigManager::mapOffsetY;
    m_winSize = ConfigManager::mapUiSize;
    Logger::info("Initializing overlay window: origin=({}, {}) size={} instance={}",
        m_winX, m_winY, m_winSize, static_cast<const void*>(hInst));

    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInst;
    wcex.lpszClassName = L"NarakaOverlayWindowClass";
    wcex.hbrBackground = CreateSolidBrush(RGB(1, 1, 1)); // 背景刷为透明键色
    const ATOM windowClass = RegisterClassExW(&wcex);
    if (windowClass) {
        Logger::info("Overlay window class registered successfully: atom={}", windowClass);
    }
    else {
        Logger::error("Overlay window class registration failed: last_error={}", static_cast<unsigned long>(GetLastError()));
    }

    m_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wcex.lpszClassName, L"NarakaMap", WS_POPUP, m_winX, m_winY, m_winSize, m_winSize, NULL, NULL, hInst, this);

    if (m_hwnd) {
        Logger::info("Overlay window created successfully: hwnd={}", static_cast<const void*>(m_hwnd));
    }
    else {
        Logger::error("Overlay window creation failed: last_error={}", static_cast<unsigned long>(GetLastError()));
    }
    SetLayeredWindowAttributes(m_hwnd, RGB(1, 1, 1), 0, LWA_COLORKEY);
    ShowWindow(m_hwnd, SW_SHOW);
}

void OverlayWindow::updateResources(const std::vector<std::string>& keys) {
    std::ostringstream keyList;
    for (size_t index = 0; index < keys.size(); ++index) {
        if (index > 0) {
            keyList << ',';
        }
        keyList << keys[index];
    }
    Logger::info("Overlay resource update requested: map_id={} key_count={} keys=[{}]",
        m_currentMapId, keys.size(), keyList.str());
    m_activeKeys = keys;
    loadData();
    invalidate();
}

void OverlayWindow::loadData() {
    const fs::path dataPath = fs::u8path(ConfigManager::resourcePath) / "resources_naraka.json";
    Logger::debug("Loading resource data: map_id={} file={} active_key_count={}",
        m_currentMapId, dataPath.string(), m_activeKeys.size());
    std::ifstream f(dataPath);
    if (!f.is_open()) {
        Logger::error("Failed to open resource data file: {}", dataPath.string());
        return;
    }
    try {
        nlohmann::json data; f >> data;
        m_points.clear();
        size_t matchingMapEntries = 0;
        size_t selectedTypeEntries = 0;
        for (auto& item : data) {
            if (item.value("map", "") == m_currentMapId) {
                ++matchingMapEntries;
                for (const auto& key : m_activeKeys) {
                    if (item.contains(key)) {
                        ++selectedTypeEntries;
                        size_t markerIndex = 0;
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
                            std::string id = marker.value("img", "");
                            if (id.empty()) {
                                std::ostringstream oss;
                                oss << m_currentMapId << ':' << key << ':' << markerIndex << ':' << gx << ',' << gy;
                                id = oss.str();
                            }
                            m_points.push_back({ id, key, ax, ay });
                            ++markerIndex;
                        }
                    }
                }
            }
        }

        std::set<std::string> validIds;
        for (const auto& point : m_points) validIds.insert(point.id);
        for (auto it = m_excludedPointIds.begin(); it != m_excludedPointIds.end();) {
            it = validIds.count(*it) ? std::next(it) : m_excludedPointIds.erase(it);
        }
        if (!m_routeStartId.empty() && !validIds.count(m_routeStartId)) {
            m_routeStartId.clear();
        }
        rebuildRoute();
        Logger::info("Resource data loaded: map_id={} map_entries={} matched_resource_types={} active_points={} excluded_points={} route_points={}",
            m_currentMapId, matchingMapEntries, selectedTypeEntries, m_points.size(), m_excludedPointIds.size(), m_routeOrder.size());
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
    Logger::info("Auto-navigation requested: cursor=({}, {}) overlay=({}, {}, {}) active_points={}",
        cur.x, cur.y, m_winX, m_winY, m_winSize, m_points.size());

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
            Logger::info("Starting auto-navigation: id={} type={} target=({}, {}) distance={:.2f}",
                target->id, target->type, target->absX, target->absY, min_dist);

            // Step 1: 模拟滚轮向上滚动 50 次 (times=50)
            SetCursorPos(target->absX, target->absY);
            Logger::debug("Auto-navigation step 1: scrolling at target position, repeats=50.");
            Win32_MouseScroll(true, 1, 50);

            // Step 2: 在目标点双击
            Win32_MouseClick(target->absX, target->absY);
            Logger::debug("Auto-navigation step 2: clicked target marker.");

            // Step 3: 等待 100ms
            Sleep(50);

            // Step 4: 点击确认按钮 (1358, 1207)
            // 这里是根据 Python 脚本中 const_var.g_地图是否展开判定点 坐标来确定的
            Win32_MouseClick(ConfigManager::detectorX, ConfigManager::detectorY);
            Logger::debug("Auto-navigation step 4: clicked confirmation point ({}, {}).",
                ConfigManager::detectorX, ConfigManager::detectorY);
            
            // Step 5: 等待 100ms
            Sleep(50);

            // Step 6: 点击 ESC 键 (VK_ESCAPE)
            Win32_KeyPress(VK_ESCAPE);
            Logger::debug("Auto-navigation step 6: sent Escape key.");

            Logger::info("Auto-navigation completed successfully for resource id={}", target->id);
        }
        else {
            Logger::warn("Auto-navigation skipped: nearest_marker_found={} nearest_distance={:.2f} threshold=100.00",
                target != nullptr, min_dist);
        }
    }
    else {
        Logger::warn("Auto-navigation skipped: cursor is outside the overlay bounds.");
    }
}

void OverlayWindow::toggleRouteVisible() {
    m_showRoute = !m_showRoute;
    rebuildRoute();
    invalidate();
    Logger::info("Marker route visibility: {}", m_showRoute ? "on" : "off");
}

void OverlayWindow::resetRoute() {
    Logger::info("Resetting marker route: excluded_points={} start_id={} route_points={}",
        m_excludedPointIds.size(), m_routeStartId, m_routeOrder.size());
    m_excludedPointIds.clear();
    m_routeStartId.clear();
    rebuildRoute();
    invalidate();
    Logger::info("Marker route state reset.");
}

void OverlayWindow::setNearestPointAsRouteStart() {
    int nearestIndex = findNearestPointToCursor(ROUTE_POINT_HIT_RADIUS);
    if (nearestIndex < 0) {
        Logger::debug("Route start skipped: no marker near cursor.");
        return;
    }

    const Point& point = m_points[nearestIndex];
    m_routeStartId = point.id;
    m_excludedPointIds.erase(point.id);
    m_showRoute = true;
    rebuildRoute();
    invalidate();
    Logger::info("Route start set: {} ({})", point.type, point.id);
}

void OverlayWindow::toggleNearestPointExcluded() {
    int nearestIndex = findNearestPointToCursor(ROUTE_POINT_HIT_RADIUS);
    if (nearestIndex < 0) {
        Logger::debug("Route exclude skipped: no marker near cursor.");
        return;
    }

    const Point& point = m_points[nearestIndex];
    if (m_excludedPointIds.count(point.id)) {
        m_excludedPointIds.erase(point.id);
        Logger::info("Route marker restored: {} ({})", point.type, point.id);
    }
    else {
        m_excludedPointIds.insert(point.id);
        if (m_routeStartId == point.id) {
            m_routeStartId.clear();
        }
        Logger::info("Route marker excluded: {} ({})", point.type, point.id);
    }

    m_showRoute = true;
    rebuildRoute();
    invalidate();
}

int OverlayWindow::findNearestPointToCursor(double maxDistance) const {
    POINT cur;
    GetCursorPos(&cur);

    if (cur.x < m_winX || cur.x > m_winX + m_winSize ||
        cur.y < m_winY || cur.y > m_winY + m_winSize) {
        return -1;
    }

    double bestDistSq = maxDistance * maxDistance;
    int bestIndex = -1;
    for (size_t i = 0; i < m_points.size(); ++i) {
        const auto& point = m_points[i];
        double dx = static_cast<double>(point.absX - cur.x);
        double dy = static_cast<double>(point.absY - cur.y);
        double distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

void OverlayWindow::rebuildRoute() {
    m_routeOrder.clear();
    if (!m_showRoute || m_points.size() < 2) return;

    std::vector<ScreenCoord> coords;
    coords.reserve(m_points.size());
    for (const auto& point : m_points) {
        coords.emplace_back(point.absX, point.absY);
    }

    std::vector<size_t> candidates;
    candidates.reserve(m_points.size());
    size_t startIndex = (std::numeric_limits<size_t>::max)();

    for (size_t i = 0; i < m_points.size(); ++i) {
        const auto& point = m_points[i];
        if (m_excludedPointIds.count(point.id)) continue;
        if (!m_routeStartId.empty() && point.id == m_routeStartId) {
            startIndex = i;
        }
        candidates.push_back(i);
    }

    if (candidates.size() < 2) return;
    if (startIndex == (std::numeric_limits<size_t>::max)()) {
        startIndex = candidates.front();
    }

    m_routeOrder = BuildNearestNeighborRoute(coords, candidates, startIndex);
    OptimizeRoute2Opt(coords, m_routeOrder, ROUTE_OPTIMIZE_BUDGET_MS);
    Logger::debug("Marker route rebuilt: candidates={} excluded_points={} start_id={} route_points={} optimize_budget_ms={}",
        candidates.size(), m_excludedPointIds.size(), m_routeStartId, m_routeOrder.size(), ROUTE_OPTIMIZE_BUDGET_MS);
}

void OverlayWindow::drawRoute(Graphics& g) {
    if (!m_showRoute || m_routeOrder.size() < 2) return;

    std::vector<PointF> points;
    points.reserve(m_routeOrder.size());
    for (size_t index : m_routeOrder) {
        if (index >= m_points.size()) continue;
        const auto& point = m_points[index];
        points.emplace_back(
            static_cast<REAL>(point.absX - m_winX),
            static_cast<REAL>(point.absY - m_winY)
        );
    }
    if (points.size() < 2) return;

    Pen shadowPen(Color(160, 6, 20, 26), 7.0f);
    shadowPen.SetLineJoin(LineJoinRound);
    shadowPen.SetStartCap(LineCapRound);
    shadowPen.SetEndCap(LineCapRound);
    g.DrawLines(&shadowPen, points.data(), static_cast<INT>(points.size()));

    Pen routePen(Color(255, 56, 189, 248), 4.0f);
    routePen.SetDashStyle(DashStyleDash);
    routePen.SetLineJoin(LineJoinRound);
    routePen.SetStartCap(LineCapRound);
    routePen.SetEndCap(LineCapRound);
    g.DrawLines(&routePen, points.data(), static_cast<INT>(points.size()));
}

void OverlayWindow::drawRouteMarkerState(Graphics& g, const Point& pt, int localX, int localY) {
    if (!m_showRoute) return;

    if (pt.id == m_routeStartId) {
        Pen startPen(Color(255, 56, 189, 248), 3.0f);
        g.DrawEllipse(&startPen, localX - 17, localY - 17, 34, 34);
    }

    if (m_excludedPointIds.count(pt.id)) {
        Pen excludedPen(Color(230, 230, 57, 70), 3.0f);
        g.DrawLine(&excludedPen, localX - 13, localY - 13, localX + 13, localY + 13);
        g.DrawLine(&excludedPen, localX + 13, localY - 13, localX - 13, localY + 13);
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

    drawRoute(g);

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
        drawRouteMarkerState(g, pt, localX, localY);
    }

    // 4. 绘制红边框
    Pen redPen(Color::Red, 2);
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
        Logger::info("Overlay visibility changed: visible={} hwnd={}", visible, static_cast<const void*>(m_hwnd));
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

bool OverlayWindow::isVisible() {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void OverlayWindow::setShowBackground(bool show) {
    Logger::info("Overlay background visibility changed: show={} map_id={} background_loaded={}",
        show, m_currentMapId, m_bgImg != nullptr);
    m_showBackground = show;
    invalidate();
}

void OverlayWindow::invalidate() {
    if (m_hwnd) InvalidateRect(m_hwnd, NULL, TRUE);
}
