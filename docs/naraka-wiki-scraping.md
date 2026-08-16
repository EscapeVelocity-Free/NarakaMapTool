# naraka.wiki 地图抓取记录

本文记录从 `https://naraka.wiki/map` 抓取地图背景和坐标数据的入口，方便后续新增地图时复用。

## 数据入口

页面本身是 SvelteKit 应用，直接打开 `https://naraka.wiki/map` 只能看到壳。可用入口：

- 页面数据：`https://naraka.wiki/map/__data.json`
- 前端入口：`https://naraka.wiki/_app/immutable/entry/app.<hash>.js`
- 地图功能 chunk：从前端入口中查找包含 `tileLayer(`/map/tiles/` 的 chunk

2026-07-08 当前版本中，入口文件是 `app.Cw3XaxS3.js`，地图 chunk 是 `chunks/BJnLDLCN.js`。这些 hash 会随站点更新变化，不要写死在脚本里。

当前已确认的站点地图编号：

| naraka.wiki id | 英文名 | 中文名 |
| --- | --- | --- |
| `0` | Morus Isle | 莫鲁斯岛 |
| `1` | Holoroth | 霍洛罗斯 |
| `2` | Perdoria | 佩多利亚 |
| `3` | Windswept Holoroth | 风蚀霍洛罗斯 |
| `4` | Rivers Runs Red | 赤水河 |
| `5` | Wanchu | 宛渠 |
| `6` | Dafeng Song | 大风歌 |

注意：站点地图 id 不一定等于本项目 UI 里的 `mapId`，接入代码前要先看 `src/control_panel.cpp` 和 `src/overlay_window.cpp` 的当前映射。

## 抓取背景地图

站点提供两套背景资源：

- 整图：`https://naraka.wiki/map/tiles/{wikiMapId}/map.png`
- 普通地图地表瓦片：`https://naraka.wiki/map/tiles/{wikiMapId}/{z}/{x}/{y}.avif`
- 大风歌地下层瓦片：`https://naraka.wiki/map/tiles/6/underground/{z}/{x}/{y}.avif`

瓦片大小是 `256x256`。当前已确认最高精度是 `z=5`，即 `32x32` 张瓦片，拼出来是 `8192x8192`。如果只需要运行时轻量背景，可以用 `z=3` 拼 `2048x2048`。

仓库里已有脚本可抓取最高精度背景：

```powershell
# 抓取 6 张地图，输出到 map/highres，默认 zoom=5
python scripts\fetch_naraka_wiki_maps.py

# 只抓宛渠
python scripts\fetch_naraka_wiki_maps.py --maps wanchu

```

## 运行时瓦片目录

程序运行时优先读取 `map/tiles` 下的 JPG/PNG 瓦片，按视口异步加载；没有瓦片或瓦片尚未加载完成时，继续使用 `map/<file>.png` 的 2048 背景兜底。瓦片目录约定如下：

```text
map/tiles/<wikiMapId>/<surface|underground>/<z>/<x>/<y>.jpg
```

抓取地表和大风歌地下层的全部运行时精度（z=3..5，最高精度 z=5）：

```powershell
python scripts\fetch_naraka_wiki_maps.py --runtime-tiles --layers all
```

只抓一张地图，适合先验证：

```powershell
python scripts\fetch_naraka_wiki_maps.py --runtime-tiles --maps perdoria --min-zoom 3 --max-zoom 5
```

瓦片下载自站点 AVIF，脚本会转换为质量 95 的 JPG，避免运行时依赖 AVIF 解码器。`--workers` 可控制并发数，`--overwrite` 强制重新抓取；默认会跳过尺寸正确的已有瓦片。

示例：抓取宛渠 `wikiMapId=5` 的 `2048x2048` 背景。

```powershell
@'
import requests
import warnings
from io import BytesIO
from pathlib import Path
from PIL import Image

warnings.filterwarnings("ignore")

wiki_map_id = 5
zoom = 3
tile_size = 256
tile_count = 2 ** zoom
base_url = f"https://naraka.wiki/map/tiles/{wiki_map_id}/{zoom}"
output = Path("map/wanchu.png")

canvas = Image.new("RGBA", (tile_count * tile_size, tile_count * tile_size))

for x in range(tile_count):
    for y in range(tile_count):
        url = f"{base_url}/{x}/{y}.avif"
        response = requests.get(url, verify=False, timeout=30)
        response.raise_for_status()
        tile = Image.open(BytesIO(response.content)).convert("RGBA")
        canvas.paste(tile, (x * tile_size, y * tile_size))

canvas.save(output)
print(output, canvas.size)
'@ | python -
```

如果只需要快速核对地图，可以直接抓整图后缩放到 `2048x2048`：

```powershell
@'
import requests
import warnings
from io import BytesIO
from pathlib import Path
from PIL import Image

warnings.filterwarnings("ignore")

wiki_map_id = 5
url = f"https://naraka.wiki/map/tiles/{wiki_map_id}/map.png"
output = Path("map/wanchu.png")

image = Image.open(BytesIO(requests.get(url, verify=False, timeout=60).content)).convert("RGBA")
image = image.resize((2048, 2048), Image.Resampling.LANCZOS)
image.save(output)
print(output, image.size)
'@ | python -
```

## 查找坐标数据

坐标数据不在 `__data.json` 里。它被打进前端 chunk 中，当前能通过如下方式定位：

1. 抓取 `https://naraka.wiki/_app/immutable/entry/app.<hash>.js`
2. 从其中提取 `../chunks/*.js` 和 `../nodes/*.js`
3. 搜索资源类型字符串，例如 `goldenToad`、`firefly`、`wishingWell`、`questCache`
4. 当前地图数据 chunk 可通过这些特征确认：
   - 包含 `tileLayer(`/map/tiles/${e}/{z}/{x}/{y}.avif`
   - 包含 `MORUS_ISLE`、`HOLOROTH`、`RIVERS_RUNS_RED`
   - 包含资源点 JSON，例如 `"type":"firefly","pos":[...],"img":"0/firefly/..."`

2026-07-08 当前版本中：

- 资源类型/图标定义变量是 `qe = JSON.parse(...)`
- 坐标数据变量是 `it = JSON.parse(...)`
- `it` 数组长度是 `6`
- `it[5]` 是宛渠数据，当前正式数据为 `129` 个点

可用下面的脚本片段从最新页面入口自动找 chunk，并提取图标定义与坐标数据：

```powershell
@'
import json
import re
import requests
import warnings

warnings.filterwarnings("ignore")

html = requests.get("https://naraka.wiki/map", verify=False, timeout=60).text
app_match = re.search(r'import\("\./_app/immutable/entry/app\.([^"]+)"\)', html)
app_url = "https://naraka.wiki/_app/immutable/entry/app." + app_match.group(1)
app = requests.get(app_url, verify=False, timeout=60).text

files = sorted(set(re.findall(r'\.\./(?:chunks|nodes)/[^"\']+\.js', app)))
chunk_text = None
for file in files:
    url = "https://naraka.wiki/_app/immutable/" + file.replace("../", "")
    text = requests.get(url, verify=False, timeout=60).text
    if "MarkerList" in text and "iconUrl" in text and "tileLayer(`/map/tiles/" in text:
        chunk_text = text
        print("map chunk:", file)
        break

icons = None
maps = None
for match in re.finditer(r'var ([A-Za-z_$][\w$]*)=JSON\.parse\(`(.*?)`\)', chunk_text, re.S):
    name, raw = match.group(1), match.group(2)
    data = json.loads(raw)
    if isinstance(data, list) and data and isinstance(data[0], dict):
        if any("iconUrl" in item for item in data if isinstance(item, dict)):
            icons = data
            print("icons variable:", name, len(data))
        if any(any(isinstance(v, dict) and "MarkerList" in v for v in item.values()) for item in data if isinstance(item, dict)):
            maps = data
            print("maps variable:", name, len(data))

print("wanchu keys:", sorted(maps[5].keys()))
print("wanchu points:", sum(len(v["MarkerList"]) for v in maps[5].values()))
'@ | python -
```

## 抓取图标

`qe` 里的 `iconUrl` 形如 `icons/xxx.png`，但实际 URL 在地图页面路径下，通常应请求：

```text
https://naraka.wiki/map/{iconUrl}
```

例如 `helanArt` 的定义是 `icons/helan_art.png`，实际可下载地址是：

```text
https://naraka.wiki/map/icons/helan_art.png
```

下载到本项目后放在：

```text
resources/icons/helan_art.png
```

同时需要在 `src/overlay_window.cpp` 的 `GetMappingFileName()` 中补类型映射：

```cpp
{"helanArt", "helan_art"}
```

注意：`resources/resources_naraka_new.json` 和 `resources/resources_naraka_new_test.json` 是临时抓取文件，按当前约定不要作为接入来源，也不要在补正式数据时修改它们。

## 本项目接入位置

新增背景图后通常需要检查这些位置：

- `map/<file>.png`：运行时背景图，当前运行用宛渠是 `map/wanchu.png`
- `map/highres/<file>.png`：最高精度背景图，`z=5` 拼接得到 `8192x8192`
- `src/control_panel.cpp`：地图下拉框是否有对应地图项
- `src/overlay_window.cpp`：`ResolveMapBackgroundName()` 是否把项目 `mapId` 映射到正确背景文件
- `src/overlay_window.cpp`：`GetMappingFileName()` 是否包含新增资源类型到图标文件名的映射
- `resources/icons/<file>.png`：新增资源图标
- `resources/resources_naraka.json`：正式坐标数据；当前宛渠使用 map `5`
