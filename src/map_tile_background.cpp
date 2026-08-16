#include "map_tile_background.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <system_error>

#include "logger.h"

namespace fs = std::filesystem;
using namespace Gdiplus;

MapTileBackground::MapTileBackground(std::size_t cacheCapacity)
    : m_cacheCapacity((std::max)(1u, static_cast<unsigned int>(cacheCapacity))) {}

MapTileBackground::~MapTileBackground() {
    stop();

    // GDI+ images must be released before GdiplusShutdown is called by the
    // owner. The normal owner lifecycle calls this while GDI+ is still alive.
    m_cache.clear();
    m_lru.clear();
    m_completed.clear();
}

void MapTileBackground::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_worker.joinable()) {
        return;
    }

    m_stop = false;
    m_worker = std::thread(&MapTileBackground::workerMain, this);
    Logger::info("Map tile worker started: cache_capacity={}", m_cacheCapacity);
}

void MapTileBackground::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_worker.joinable()) {
            return;
        }
        m_stop = true;
        m_pending.clear();
        m_queuedKeys.clear();
        m_inFlightKeys.clear();
    }

    m_cv.notify_all();
    m_worker.join();
    m_notificationPending.store(false, std::memory_order_release);
    Logger::info("Map tile worker stopped.");
}

void MapTileBackground::setNotifyWindow(HWND hwnd) {
    m_notifyWindow.store(hwnd, std::memory_order_release);
}

void MapTileBackground::setRootPath(const fs::path& rootPath) {
    m_rootPath = rootPath;
    Logger::debug("Map tile root configured: {}", m_rootPath.string());
}

void MapTileBackground::setMap(const std::string& mapId, int layer) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_generation;
        m_pending.clear();
        m_queuedKeys.clear();
        m_inFlightKeys.clear();
        m_completed.clear();
        m_viewportRevision = 0;
        m_viewportZoom = -1;
        m_viewportFirstX = -1;
        m_viewportLastX = -1;
        m_viewportFirstY = -1;
        m_viewportLastY = -1;
    }

    m_mapId = mapId;
    m_layer = layer;
    m_cache.clear();
    m_lru.clear();
    refreshAvailableZooms();

    Logger::info("Map tile context changed: map_id={} layer={} available_zooms={} max_zoom={} cache_cleared=true",
        m_mapId, m_layer, m_availableZooms.size(), m_maxZoom);
}

void MapTileBackground::clearCache() {
    m_cache.clear();
    m_lru.clear();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completed.clear();
}

void MapTileBackground::beginViewportRequest(
    int zoom, int firstX, int lastX, int firstY, int lastY) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_viewportZoom == zoom && m_viewportFirstX == firstX &&
        m_viewportLastX == lastX && m_viewportFirstY == firstY &&
        m_viewportLastY == lastY) {
        return;
    }

    m_viewportZoom = zoom;
    m_viewportFirstX = firstX;
    m_viewportLastX = lastX;
    m_viewportFirstY = firstY;
    m_viewportLastY = lastY;
    ++m_viewportRevision;
    m_pending.clear();
    m_queuedKeys.clear();
}

void MapTileBackground::request(const TileKey& key) {
    if (key.zoom < 0 || key.zoom > kMaxSupportedZoom || key.x < 0 || key.y < 0) {
        return;
    }
    const int tileCount = 1 << key.zoom;
    if (key.x >= tileCount || key.y >= tileCount || isCached(key)) {
        return;
    }

    TileRequest request;
    request.key = key;
    request.path = tilePathFor(key);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        request.generation = m_generation;
        request.viewportRevision = m_viewportRevision;
        const RequestKey requestKey{request.generation, key};
        if (m_queuedKeys.count(requestKey) != 0 || m_inFlightKeys.count(requestKey) != 0) {
            return;
        }
        m_queuedKeys.insert(requestKey);
        m_pending.emplace_back(std::move(request));
    }
    m_cv.notify_one();
}

MapTileBackground::TileImage MapTileBackground::find(const TileKey& key) {
    const auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        return nullptr;
    }

    m_lru.splice(m_lru.end(), m_lru, it->second.lruPosition);
    it->second.lruPosition = std::prev(m_lru.end());
    return it->second.image;
}

bool MapTileBackground::isCached(const TileKey& key) const {
    return m_cache.find(key) != m_cache.end();
}

std::size_t MapTileBackground::drainCompleted() {
    std::deque<TileResult> results;
    std::uint64_t currentGeneration = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentGeneration = m_generation;
        results.swap(m_completed);
        for (const auto& result : results) {
            const RequestKey requestKey{result.generation, result.key};
            m_queuedKeys.erase(requestKey);
            m_inFlightKeys.erase(requestKey);
        }
    }

    std::size_t applied = 0;
    for (const auto& result : results) {
        if (result.generation != currentGeneration) {
            continue;
        }
        cacheResult(result);
        ++applied;
    }

    m_notificationPending.store(false, std::memory_order_release);

    // A worker can finish between the queue swap and the notification reset.
    // Recheck the queue after resetting the flag so that no wake-up is lost.
    bool hasMoreResults = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        hasMoreResults = !m_completed.empty();
    }
    if (hasMoreResults) {
        notifyMainThread();
    }

    if (applied > 0) {
        m_resultsSinceLastLog += applied;
        const ULONGLONG now = GetTickCount64();
        if (m_lastResultsLogTick == 0 || now - m_lastResultsLogTick >= 1000) {
            Logger::debug("Map tile results applied: count={} cache_size={} map_id={} layer={}",
                m_resultsSinceLastLog, m_cache.size(), m_mapId, m_layer);
            m_lastResultsLogTick = now;
            m_resultsSinceLastLog = 0;
        }
    }
    return applied;
}

int MapTileBackground::preferredZoom(double mapScreenSize) const {
    if (m_maxZoom < 0 || m_availableZooms.empty()) {
        return -1;
    }

    // Match source resolution to the displayed full-map size. At the default
    // view this selects z=3; zooming in naturally advances to z=4 and z=5.
    const double safeSize = (std::max)(1.0, mapScreenSize);
    int desiredZoom = static_cast<int>(std::ceil(std::log2(safeSize))) - 8;
    desiredZoom = (std::max)(0, (std::min)(kMaxSupportedZoom, desiredZoom));

    for (int availableZoom : m_availableZooms) {
        if (availableZoom >= desiredZoom) {
            return availableZoom;
        }
    }
    return m_maxZoom;
}

int MapTileBackground::maxZoom() const {
    return m_maxZoom;
}

bool MapTileBackground::hasTiles() const {
    return m_maxZoom >= 0;
}

std::size_t MapTileBackground::cachedTileCount() const {
    return m_cache.size();
}

void MapTileBackground::workerMain() {
    for (;;) {
        TileRequest request;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stop || !m_pending.empty(); });
            if (m_stop && m_pending.empty()) {
                return;
            }

            request = std::move(m_pending.front());
            m_pending.pop_front();
            const RequestKey requestKey{request.generation, request.key};
            m_queuedKeys.erase(requestKey);
            m_inFlightKeys.insert(requestKey);
        }

        TileImage image;
        try {
            const fs::path path = resolveTilePath(request.path);
            if (fs::exists(path)) {
                Image* rawImage = Image::FromFile(path.wstring().c_str());
                if (rawImage && rawImage->GetLastStatus() == Ok) {
                    image.reset(rawImage);
                }
                else {
                    delete rawImage;
                }
            }
        }
        catch (const std::exception& e) {
            Logger::warn("Map tile decode failed: path={} error={}", request.path.string(), e.what());
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_stop) {
                m_completed.push_back(TileResult{request.key, request.generation, std::move(image)});
            }
        }
        notifyMainThread();
    }
}

void MapTileBackground::notifyMainThread() {
    if (m_notificationPending.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    const HWND hwnd = m_notifyWindow.load(std::memory_order_acquire);
    if (!hwnd || !PostMessageW(hwnd, WM_APP + 2, 0, 0)) {
        m_notificationPending.store(false, std::memory_order_release);
    }
}

void MapTileBackground::cacheResult(const TileResult& result) {
    const auto existing = m_cache.find(result.key);
    if (existing != m_cache.end()) {
        existing->second.image = result.image;
        m_lru.splice(m_lru.end(), m_lru, existing->second.lruPosition);
        existing->second.lruPosition = std::prev(m_lru.end());
        return;
    }

    m_lru.push_back(result.key);
    m_cache.emplace(result.key, CacheEntry{result.image, std::prev(m_lru.end())});

    while (m_cache.size() > m_cacheCapacity) {
        const TileKey oldest = m_lru.front();
        m_lru.pop_front();
        m_cache.erase(oldest);
    }
}

fs::path MapTileBackground::tilePathFor(const TileKey& key) const {
    const std::string layerName = m_layer == 1 ? "underground" : "surface";
    const fs::path layeredPath = m_rootPath / "tiles" / m_mapId / layerName /
        std::to_string(key.zoom) / std::to_string(key.x) / (std::to_string(key.y) + ".png");
    if (fs::exists(layeredPath)) {
        return layeredPath;
    }

    // The scraper may store runtime tiles as JPG. Do not use tileExists here:
    // it intentionally accepts alternate extensions, but returning the PNG
    // candidate after finding a JPG would make the decoder open a nonexistent
    // file and silently force the low-resolution background fallback.
    fs::path jpgPath = layeredPath;
    jpgPath.replace_extension(".jpg");
    if (fs::exists(jpgPath)) {
        return jpgPath;
    }
    fs::path jpegPath = layeredPath;
    jpegPath.replace_extension(".jpeg");
    if (fs::exists(jpegPath)) {
        return jpegPath;
    }

    // Keep compatibility with the first generation of the scraper, which did
    // not include a surface directory for single-layer maps.
    if (m_layer == 0) {
        const fs::path legacyPath = m_rootPath / "tiles" / m_mapId / std::to_string(key.zoom) /
            std::to_string(key.x) / (std::to_string(key.y) + ".png");
        if (fs::exists(legacyPath)) {
            return legacyPath;
        }
        fs::path legacyJpgPath = legacyPath;
        legacyJpgPath.replace_extension(".jpg");
        if (fs::exists(legacyJpgPath)) {
            return legacyJpgPath;
        }
        fs::path legacyJpegPath = legacyPath;
        legacyJpegPath.replace_extension(".jpeg");
        if (fs::exists(legacyJpegPath)) {
            return legacyJpegPath;
        }
    }
    return layeredPath;
}

fs::path MapTileBackground::resolveTilePath(const fs::path& basePath) const {
    std::error_code error;
    if (fs::exists(basePath, error)) {
        return basePath;
    }
    fs::path jpgPath = basePath;
    jpgPath.replace_extension(".jpg");
    if (fs::exists(jpgPath, error)) {
        return jpgPath;
    }
    fs::path jpegPath = basePath;
    jpegPath.replace_extension(".jpeg");
    if (fs::exists(jpegPath, error)) {
        return jpegPath;
    }
    return basePath;
}

bool MapTileBackground::tileExists(const fs::path& basePath) const {
    std::error_code error;
    if (fs::exists(basePath, error)) {
        return true;
    }

    fs::path jpgPath = basePath;
    jpgPath.replace_extension(".jpg");
    if (fs::exists(jpgPath, error)) {
        return true;
    }
    fs::path jpegPath = basePath;
    jpegPath.replace_extension(".jpeg");
    return fs::exists(jpegPath, error);
}

void MapTileBackground::refreshAvailableZooms() {
    m_availableZooms.clear();
    for (int zoom = 0; zoom <= kMaxSupportedZoom; ++zoom) {
        const TileKey firstTile{zoom, 0, 0};
        if (tileExists(tilePathFor(firstTile))) {
            m_availableZooms.push_back(zoom);
        }
    }
    m_maxZoom = m_availableZooms.empty() ? -1 : m_availableZooms.back();
}
