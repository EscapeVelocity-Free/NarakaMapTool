#include "resource_catalog.h"

#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "logger.h"

namespace {
using Json = nlohmann::json;

std::string ReadMapId(const Json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<int>());
    }
    return {};
}

int ReadLayer(const Json& marker) {
    if (!marker.is_object() || !marker.contains("layer")) {
        return 0;
    }

    const auto& layer = marker["layer"];
    if (layer.is_number_integer()) {
        return layer.get<int>();
    }
    return 0;
}
}

ResourceCatalog::ResourceCatalog(std::string dataPath) : m_dataPath(std::move(dataPath)) {
    reload();
}

bool ResourceCatalog::reload() {
    std::ifstream input(m_dataPath);
    if (!input.is_open()) {
        Logger::warn("Resource catalog file could not be opened: path={}", m_dataPath);
        return false;
    }

    try {
        Json document;
        input >> document;
        if (!document.is_array()) {
            Logger::warn("Resource catalog has an unexpected root type: path={}", m_dataPath);
            return false;
        }

        std::unordered_map<std::string, LayerCounts> counts;
        std::size_t markerCount = 0;
        std::size_t resourceCount = 0;

        for (const auto& mapData : document) {
            if (!mapData.is_object() || !mapData.contains("map")) {
                continue;
            }

            const std::string mapId = ReadMapId(mapData["map"]);
            if (mapId.empty()) {
                continue;
            }

            for (auto it = mapData.begin(); it != mapData.end(); ++it) {
                if (it.key() == "map" || !it.value().is_object() ||
                    !it.value().contains("MarkerList") || !it.value()["MarkerList"].is_array()) {
                    continue;
                }

                const auto& markers = it.value()["MarkerList"];
                if (markers.empty()) {
                    continue;
                }

                ++resourceCount;
                for (const auto& marker : markers) {
                    ++counts[mapId][ReadLayer(marker)][it.key()];
                    ++markerCount;
                }
            }
        }

        m_counts = std::move(counts);
        m_loaded = true;
        Logger::info("Resource catalog loaded: path={} maps={} resource_types={} points={}",
            m_dataPath, m_counts.size(), resourceCount, markerCount);
        return true;
    } catch (const std::exception& error) {
        Logger::warn("Resource catalog parsing failed: path={} error={}", m_dataPath, error.what());
        return false;
    }
}

bool ResourceCatalog::isAvailable(const std::string& mapId, const std::string& resourceKey, int layer) const {
    if (!m_loaded) {
        // Keep the previous fail-open behavior if the optional catalog cannot be read.
        return true;
    }

    const auto mapIt = m_counts.find(mapId);
    if (mapIt == m_counts.end()) {
        return false;
    }

    if (layer >= 0) {
        const auto layerIt = mapIt->second.find(layer);
        return layerIt != mapIt->second.end() && layerIt->second.find(resourceKey) != layerIt->second.end();
    }

    for (const auto& [ignoredLayer, resources] : mapIt->second) {
        if (resources.find(resourceKey) != resources.end()) {
            return true;
        }
    }
    return false;
}

std::size_t ResourceCatalog::pointCount(const std::string& mapId, const std::string& resourceKey, int layer) const {
    if (!m_loaded) {
        return 0;
    }

    const auto mapIt = m_counts.find(mapId);
    if (mapIt == m_counts.end()) {
        return 0;
    }

    if (layer >= 0) {
        const auto layerIt = mapIt->second.find(layer);
        if (layerIt == mapIt->second.end()) {
            return 0;
        }
        const auto resourceIt = layerIt->second.find(resourceKey);
        return resourceIt == layerIt->second.end() ? 0 : resourceIt->second;
    }

    std::size_t total = 0;
    for (const auto& [ignoredLayer, resources] : mapIt->second) {
        const auto resourceIt = resources.find(resourceKey);
        if (resourceIt != resources.end()) {
            total += resourceIt->second;
        }
    }
    return total;
}

bool ResourceCatalog::isLoaded() const {
    return m_loaded;
}
