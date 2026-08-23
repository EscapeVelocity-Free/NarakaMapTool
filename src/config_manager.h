#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>

class ConfigManager {
public:
    // 最终生效的参数
    static int mapOffsetX;
    static int mapOffsetY;
    static int mapUiSize;
    static int detectorX;
    static int detectorY;

    // 路径
    static std::string resourcePath;
    static std::string mapImagePath;

    static bool quickPanelEnabled;
    static std::string quickPanelHotkey;

    // 初始化：先动态计算，再尝试文件覆盖
    static void init(const std::string& filename);
    static bool saveQuickPanelSettings(bool enabled, const std::string& hotkey);
};

#endif
