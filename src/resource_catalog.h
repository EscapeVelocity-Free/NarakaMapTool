#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

class ResourceCatalog {
public:
    explicit ResourceCatalog(std::string dataPath);

    bool reload();
    bool isAvailable(const std::string& mapId, const std::string& resourceKey, int layer = -1) const;
    std::size_t pointCount(const std::string& mapId, const std::string& resourceKey, int layer = -1) const;
    bool isLoaded() const;

private:
    using ResourceCounts = std::unordered_map<std::string, std::size_t>;
    using LayerCounts = std::unordered_map<int, ResourceCounts>;

    std::string m_dataPath;
    std::unordered_map<std::string, LayerCounts> m_counts;
    bool m_loaded = false;
};
