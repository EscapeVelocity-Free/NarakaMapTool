#ifndef MAP_TILE_BACKGROUND_H
#define MAP_TILE_BACKGROUND_H

#include <windows.h>
#include <gdiplus.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

// Loads high-resolution map tiles away from the overlay UI thread. The overlay
// owns rendering; this class only owns tile discovery, decoding, and caching.
class MapTileBackground {
public:
    static constexpr int kTileSize = 256;
    static constexpr int kMaxSupportedZoom = 5;

    struct TileKey {
        int zoom = 0;
        int x = 0;
        int y = 0;

        bool operator<(const TileKey& other) const {
            if (zoom != other.zoom) return zoom < other.zoom;
            if (x != other.x) return x < other.x;
            return y < other.y;
        }
    };

    using TileImage = std::shared_ptr<Gdiplus::Image>;

    // Keep the visible zoom level and its parent fallback tiles resident. A
    // 128-entry cache is too small when switching from z4 to z5 near the
    // viewport boundary and causes continuous eviction/reload churn.
    explicit MapTileBackground(std::size_t cacheCapacity = 512);
    ~MapTileBackground();

    MapTileBackground(const MapTileBackground&) = delete;
    MapTileBackground& operator=(const MapTileBackground&) = delete;

    void start();
    void stop();
    void setNotifyWindow(HWND hwnd);
    void setRootPath(const std::filesystem::path& rootPath);
    void setMap(const std::string& mapId, int layer);
    void clearCache();

    // Called by the UI thread for the currently visible tile range.
    // A new frame supersedes queued requests from an older viewport, while
    // an already decoding tile is allowed to finish and can still be cached.
    void beginViewportRequest(int zoom, int firstX, int lastX, int firstY, int lastY);
    void request(const TileKey& key);

    // Called by the UI thread while painting. Cache access never crosses the
    // worker boundary, so GDI+ images are only drawn after decoding completes.
    TileImage find(const TileKey& key);
    bool isCached(const TileKey& key) const;
    std::size_t drainCompleted();

    int preferredZoom(double mapScreenSize) const;
    int maxZoom() const;
    bool hasTiles() const;
    std::size_t cachedTileCount() const;

private:
    struct TileRequest {
        TileKey key;
        std::uint64_t generation = 0;
        std::uint64_t viewportRevision = 0;
        std::filesystem::path path;
    };

    struct TileResult {
        TileKey key;
        std::uint64_t generation = 0;
        TileImage image;
    };

    struct RequestKey {
        std::uint64_t generation = 0;
        TileKey key;

        bool operator<(const RequestKey& other) const {
            if (generation != other.generation) return generation < other.generation;
            return key < other.key;
        }
    };

    struct CacheEntry {
        TileImage image;
        std::list<TileKey>::iterator lruPosition;
    };

    void workerMain();
    void notifyMainThread();
    void cacheResult(const TileResult& result);
    std::filesystem::path tilePathFor(const TileKey& key) const;
    std::filesystem::path resolveTilePath(const std::filesystem::path& basePath) const;
    bool tileExists(const std::filesystem::path& basePath) const;
    void refreshAvailableZooms();

    const std::size_t m_cacheCapacity;
    std::filesystem::path m_rootPath;
    std::string m_mapId;
    int m_layer = 0;
    int m_maxZoom = -1;
    std::vector<int> m_availableZooms;

    std::map<TileKey, CacheEntry> m_cache;
    std::list<TileKey> m_lru;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<TileRequest> m_pending;
    // Keep queued and currently decoding keys separate. A new viewport can
    // discard the former without duplicating work for the latter.
    std::set<RequestKey> m_queuedKeys;
    std::set<RequestKey> m_inFlightKeys;
    std::deque<TileResult> m_completed;
    std::uint64_t m_generation = 0;
    std::uint64_t m_viewportRevision = 0;
    int m_viewportZoom = -1;
    int m_viewportFirstX = -1;
    int m_viewportLastX = -1;
    int m_viewportFirstY = -1;
    int m_viewportLastY = -1;
    bool m_stop = false;
    std::thread m_worker;

    std::atomic<HWND> m_notifyWindow{nullptr};
    std::atomic<bool> m_notificationPending{false};
    ULONGLONG m_lastResultsLogTick = 0;
    std::size_t m_resultsSinceLastLog = 0;
};

#endif
