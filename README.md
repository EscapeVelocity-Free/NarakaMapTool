# NarakaMapTool (永劫无间资源地图辅助工具)

<p align="center">
  <b>永劫无间 (Naraka: Bladepoint)</b> 第三方资源地图辅助工具 —— 在游戏内地图上显示资源点位，支持一键自动标记。
</p>

<p align="center">
  <img src="resources/icons/toad.png" width="24" alt="icon">
  <img src="resources/icons/firefly.png" width="24" alt="icon">
  <img src="resources/icons/wishing_well.png" width="24" alt="icon">
  <img src="resources/icons/bounty.png" width="24" alt="icon">
  <img src="resources/icons/soul_altar.png" width="24" alt="icon">
</p>

---

## ✨ 功能特性

- 🗺️ **三张地图全覆盖**：支持聚窟州（Morus）、火罗国（Holoroth）、龙隐洞天（Dragon's Cavern）
- 📍 **40+ 种资源标记**：萤火虫、金蟾、许愿井、悬赏、地脉仪、商店等，应有尽有
- 🔍 **实时透明覆盖层**：在游戏地图上方叠加半透明标记层，不影响游戏操作
- ⚡ **Alt 键快速标记**：打开地图后按 Alt，自动在鼠标附近的资源标记点上执行缩放和点击，快速设置导航
- 🎮 **自动检测地图开关**：像素级检测游戏地图的打开/关闭状态，覆盖层自动显隐
- 📐 **多分辨率适配**：以 2K（2560×1440）为基准自动缩放，支持不同分辨率
- 🎨 **背景地图切换**：可显示/隐藏背景地图，根据需要自由搭配
- 📂 **配置自动生成**：首次运行自动生成 `config.json`，也可手动编辑微调参数

## 🖥️ 界面预览

控制面板包含以下区域：

| 区域 | 说明 |
|------|------|
| 顶部栏 | 地图选择下拉框 + 背景地图开关 + 全部显示/全部隐藏按钮 |
| 常用消耗 & 采集 | 萤火虫、许愿井、小土地、金蟾、鸟靶、萤火笼、梨、刺梨、蛇皮果、蒲公英、锦鲤 |
| 任务 & 挑战 | 任务卷轴、任务、任务土地、任务钟、悬赏、叫阵、地脉仪 |
| 重要设施 | 商店、反魂台、武器架、宝库、奥义封印、回阳镜、食人花、雪莲、泉源 |
| 其他 | 金堆、绿堆（数据不全，实用性不高）、漂浮堆、钟、攻城弩、滴滴打车、捕兽夹、野生动物、烤火、疗愈之树、老鼠、土地庙、铜钱 |

每个资源类型都有独立的勾选框，可以自由组合显示。

## 📥 下载与安装

### 从 Release 下载（推荐）

前往 [Releases](../../releases) 页面下载最新版本的 ZIP 压缩包。

解压后的目录结构如下：

```
NarakaMapTool/
├── NarakaMapTool.exe    # 主程序
├── resources/            # 资源数据
│   ├── icons/            # 标记图标
│   └── resources_naraka.json  # 资源坐标数据库
├── map/                  # 背景地图图片
└── config.json           # 配置文件（首次运行自动生成）
```

**直接运行 `NarakaMapTool.exe` 即可使用，无需额外安装。**

### 从源码构建

如果你想自行编译，请参照以下步骤。

#### 环境要求

| 工具 | 版本要求 |
|------|----------|
| CMake | >= 3.15 |
| Conan 2 | >= 2.0 |
| Visual Studio 2022 | 带 C++ 桌面开发工作负载 |
| Python | >= 3.8（Conan 依赖） |

#### 构建步骤

```bash
# 1. 安装依赖
conan install . --output-folder=build --build=missing -s compiler.runtime=static

# 2. 配置（Conan 会自动生成 CMake Presets）
cmake --preset conan-default

# 3. 编译
cmake --build --preset conan-release

# 4. 打包（可选）
cd build\build
cpack -C Release
```

编译产物在 `build\build\Release\` 目录下。

## 🎮 使用方法

### 基本使用

1. **启动程序**：运行 `NarakaMapTool.exe`，会弹出控制面板窗口
2. **选择地图**：在顶部下拉框中选择当前游戏地图（聚窟州 / 火罗国 / 龙隐洞天）
3. **勾选资源**：在控制面板中勾选你想要显示的资源类型
4. **进入游戏**：在永劫无间中打开游戏内地图（按 M 键）
5. **查看标记**：工具会自动检测到地图已打开，在地图上显示资源标记

### Alt 快速标记导航

当你打开游戏内地图后，按住 Alt 键，工具会自动执行以下一连串操作：

1. 找到鼠标光标 **100 像素范围内** 最近的资源标记点
2. 将光标移动到该点，并模拟滚轮缩放地图
3. 点击该资源点将其选中
4. 点击确认按钮以确认导航
5. 自动按 ESC 关闭地图

> 💡 通过模拟鼠标和键盘输入，帮你快速将某个资源点设为游戏导航路线，省去手动缩放和点击的步骤。

### 配置文件

首次运行后，程序会自动在同级目录生成 `config.json`：

```json
{
    "map_offset_x": 1413,
    "map_offset_y": 149,
    "map_ui_size": 1093,
    "detector_x": 1358,
    "detector_y": 1207,
    "map_path": "./map/",
    "resource_path": "./resources/"
}
```

| 参数 | 说明 |
|------|------|
| `map_offset_x` | 地图覆盖层的 X 坐标偏移 |
| `map_offset_y` | 地图覆盖层的 Y 坐标偏移 |
| `map_ui_size` | 地图覆盖层的边长（正方形） |
| `detector_x` | 地图检测点的 X 像素坐标 |
| `detector_y` | 地图检测点的 Y 像素坐标 |
| `map_path` | 背景地图图片目录路径 |
| `resource_path` | 资源数据目录路径 |

> 💡 程序会根据当前屏幕分辨率自动计算这些参数，通常无需手动修改。如果覆盖层位置不准确，可以手动微调这些值。

## 🔧 技术架构

本项目使用 **Qt 6** 作为控制面板 UI 框架，**Win32 + GDI+** 实现高性能透明覆盖层。

```
┌──────────────────────────────────────────────────┐
│                 ControlPanel                      │
│            (Qt 6 Widgets 控制面板)                  │
│    地图选择 │ 资源勾选 │ 背景切换 │ 全部显示/隐藏      │
└──────────┬───────────────────┬───────────────────┘
           │  mapChanged        │  selectionChanged   │  toggleBackground
           ▼                   ▼                    ▼
┌──────────────────────────────────────────────────────┐
│                 OverlayWindow                        │
│           (Win32 分层透明窗口 / GDI+ 渲染)            │
│    资源图标绘制 │ 背景地图显示 │ Alt 寻路交互             │
└──────────▲──────────────────────────────────────────┘
           │  mapVisibilityChanged    altTriggered
┌──────────┴──────────────────────────────────────────┐
│              MapStatusDetector                       │
│        (QTimer 定时器 / 像素级地图状态检测)             │
└──────────────────────────────────────────────────────┘
           │
┌──────────┴──────────────────────────────────────────┐
│              ConfigManager                           │
│        (静态配置 / 分辨率自适应 / JSON 读写)           │
└──────────────────────────────────────────────────────┘
```

### 依赖库

| 库 | 版本 | 用途 |
|----|------|------|
| Qt 6 | 6.7.0 | 控制面板 UI |
| nlohmann/json | 3.11.3 | JSON 解析 |
| spdlog | 1.13.0 | 日志记录 |
| Win32 GDI+ | 系统自带 | 覆盖层渲染 |

## ❓ 常见问题

<details>
<summary><b>覆盖层位置不准确怎么办？</b></summary>

编辑 `config.json`，微调 `map_offset_x`、`map_offset_y` 和 `map_ui_size` 的值，使覆盖层与游戏内地图对齐。这些值是以 2K（2560×1440）为基准的，其他分辨率会按比例缩放。
</details>

<details>
<summary><b>为什么地图打开后没有显示标记？</b></summary>

1. 确认控制面板中已勾选了需要显示的资源类型
2. 检查 `detector_x` 和 `detector_y` 配置是否正确，这两个值决定了检测游戏地图是否打开的采样点位置
3. 查看同目录下的 `NarakaMapTool.log` 日志文件获取详细信息
</details>

<details>
<summary><b>Alt 寻路不生效？</b></summary>

确保鼠标指针在地图覆盖层范围内，且距离最近的资源标记在 100 像素以内。程序会自动找到最近的点进行导航。
</details>

<details>
<summary><b>支持哪些分辨率？</b></summary>

程序会根据当前屏幕分辨率自动缩放坐标。基准分辨率为 2560×1440（2K），其他分辨率按 `屏幕宽度 / 2560` 的比例缩放。如果缩放后的值不精确，可以手动编辑 `config.json`。
</details>

<details>
<summary><b>程序会被反作弊检测吗？</b></summary>

本工具仅通过 Win32 API 进行屏幕像素采样和模拟鼠标/键盘输入，不修改游戏内存、不注入 DLL、不 Hook 游戏进程。但使用任何第三方工具均存在一定风险，请自行评估。
</details>

## 📝 注意事项

- ⚠️ 本工具为 **第三方辅助工具**，与游戏官方无关
- ⚠️ 使用本工具产生的任何后果由用户自行承担
- ⚠️ 工具仅进行像素检测和输入模拟，**不修改游戏文件或内存**
- ⚠️ 请确保游戏以 **无边框窗口** 或 **全屏** 模式运行

## 🤝 参与贡献

欢迎贡献！你可以通过以下方式参与：

1. **提交 Issue**：报告 Bug 或提出功能建议
2. **提交 Pull Request**：修复 Bug 或添加新功能
3. **完善资源数据**：更新或补充 `resources_naraka.json` 中的资源坐标信息

## 📄 许可证

本项目基于 [MIT License](./LICENSE) 开源。

---

<p align="center">
  Made with ❤️ for Naraka: Bladepoint players
</p>
