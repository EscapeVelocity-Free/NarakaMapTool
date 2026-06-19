#include "config_manager.h"
#include <windows.h>
#include <fstream>
#include "logger.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// 定义变量
int ConfigManager::mapOffsetX = 0;
int ConfigManager::mapOffsetY = 0;
int ConfigManager::mapUiSize = 0;
int ConfigManager::detectorX = 0;
int ConfigManager::detectorY = 0;

std::string ConfigManager::resourcePath = "./resources/";
std::string ConfigManager::mapImagePath = "./map/";

void ConfigManager::init(const std::string& filename) {
    // --- 第一阶段：动态计算 (基于 2K 基准) ---
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // 计算缩放倍率 (以宽度的比例为准)
    double scale = (double)screenW / 2560.0;

    // 自动计算默认值
    mapOffsetX = (int)(1413 * scale);
    mapOffsetY = (int)(149 * scale);
    mapUiSize = (int)(1093 * scale);
    detectorX = (int)(1358 * scale);
    detectorY = (int)(1207 * scale);

    Logger::info("Screen resolution: {}x{}", screenW, screenH);
    Logger::info("Auto-adjusted offset: ({}, {})", mapOffsetX, mapOffsetY);

    // --- 第二阶段：从配置文件读取（如果存在） ---
    std::ifstream f(filename);
    if (f.is_open()) {
        try {
            json j;
            f >> j;
            if (j.contains("map_offset_x")) mapOffsetX = j["map_offset_x"];
            if (j.contains("map_offset_y")) mapOffsetY = j["map_offset_y"];
            if (j.contains("map_ui_size")) mapUiSize = j["map_ui_size"];
            if (j.contains("detector_x")) detectorX = j["detector_x"];
            if (j.contains("detector_y")) detectorY = j["detector_y"];

            if (j.contains("resource_path")) resourcePath = j["resource_path"];
            if (j.contains("map_path")) mapImagePath = j["map_path"];

            Logger::info("Config file loaded successfully, partially overridden parameters.");
        }
        catch (...) {
            Logger::error("Config file format error, please check.");
        }
    }
    else {
        // --- 第三阶段：文件不存在，自动生成默认配置文件 ---
        Logger::info("Config file not found. Generating a new one based on current resolution...");

        try {
            json j;
            j["map_offset_x"] = mapOffsetX;
            j["map_offset_y"] = mapOffsetY;
            j["map_ui_size"] = mapUiSize;
            j["detector_x"] = detectorX;
            j["detector_y"] = detectorY;
            j["resource_path"] = resourcePath;
            j["map_path"] = mapImagePath;

            std::ofstream out(filename);
            if (out.is_open()) {
                // 使用 dump(4) 生成带缩进的漂亮 JSON，方便用户手动修改
                out << j.dump(4);
                out.close();
                Logger::info("Created default config: {}", filename);
            }
            else {
                Logger::error("Failed to write config file. Check disk permissions.");
            }
        }
        catch (const std::exception& e) {
            Logger::error("Failed to generate config template: {}", e.what());
        }
    }
}