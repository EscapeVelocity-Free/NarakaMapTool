#!/usr/bin/env python3
"""
Fetch full map backgrounds from https://naraka.wiki/map.

Default behavior:
  - downloads all 7 known maps
  - uses zoom 5 tiles, 32x32 tiles, 8192x8192 output
  - writes PNG files to map/highres/

Requirements:
  pip install requests pillow
"""

from __future__ import annotations

import argparse
import concurrent.futures
import sys
import time
import warnings
from dataclasses import dataclass
from io import BytesIO
from pathlib import Path

import requests
from PIL import Image


warnings.filterwarnings("ignore")


TILE_SIZE = 256
BASE_URL = "https://naraka.wiki/map/tiles"


@dataclass(frozen=True)
class WikiMap:
    wiki_id: int
    key: str
    english_name: str
    chinese_name: str


MAPS = [
    WikiMap(0, "morus_isle", "Morus Isle", "莫鲁斯岛"),
    WikiMap(1, "holoroth", "Holoroth", "霍洛罗斯"),
    WikiMap(2, "perdoria", "Perdoria", "佩多利亚"),
    WikiMap(3, "windswept_holoroth", "Windswept Holoroth", "风蚀霍洛罗斯"),
    WikiMap(4, "rivers_runs_red", "Rivers Runs Red", "赤水河"),
    WikiMap(5, "wanchu", "Wanchu", "宛渠"),
    WikiMap(6, "da_feng_ge", "Dafeng Song", "大风歌"),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download naraka.wiki map tiles into full maps or the runtime tile tree."
    )
    parser.add_argument(
        "--output",
        default="map/highres",
        help="Output directory. Default: map/highres",
    )
    parser.add_argument(
        "--zoom",
        type=int,
        default=5,
        help="Tile zoom level (3-5). Default: 5, producing 8192x8192 maps.",
    )
    parser.add_argument(
        "--workers",
        type=int,
        default=32,
        help="Concurrent tile downloads per map. Default: 32",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=6,
        help="Retry count per tile. Default: 6",
    )
    parser.add_argument(
        "--maps",
        nargs="+",
        default=[m.key for m in MAPS],
        choices=[m.key for m in MAPS],
        help="Map keys to fetch. Default: all maps.",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing complete output files.",
    )
    parser.add_argument(
        "--runtime-tiles",
        action="store_true",
        help="Write runtime tiles to map/tiles/{map_id}/{layer}/{z}/{x}/{y}.jpg.",
    )
    parser.add_argument(
        "--tile-output",
        default="map/tiles",
        help="Runtime tile output directory. Default: map/tiles",
    )
    parser.add_argument(
        "--min-zoom",
        type=int,
        default=3,
        help="Lowest runtime tile zoom (3 produces 2048x2048). Default: 3",
    )
    parser.add_argument(
        "--max-zoom",
        type=int,
        default=5,
        help="Highest runtime tile zoom. Default: 5",
    )
    parser.add_argument(
        "--layers",
        choices=["surface", "underground", "all"],
        default="surface",
        help="Runtime layers to download. 'all' adds underground for 大风歌.",
    )
    return parser.parse_args()


def expected_size(zoom: int) -> tuple[int, int]:
    side = (2**zoom) * TILE_SIZE
    return side, side


def fetch_tile(
    session: requests.Session,
    wiki_id: int,
    layer: str | None,
    zoom: int,
    x: int,
    y: int,
    retries: int,
) -> tuple[int, int, Image.Image]:
    layer_part = f"/{layer}" if layer == "underground" else ""
    url = f"{BASE_URL}/{wiki_id}{layer_part}/{zoom}/{x}/{y}.avif"
    last_error: Exception | None = None

    for attempt in range(retries):
        try:
            response = session.get(url, verify=False, timeout=30)
            response.raise_for_status()
            tile = Image.open(BytesIO(response.content)).convert("RGB")
            if tile.size != (TILE_SIZE, TILE_SIZE):
                raise RuntimeError(f"unexpected tile size {tile.size}")
            return x, y, tile
        except Exception as exc:
            last_error = exc
            time.sleep(0.5 + attempt * 0.5)

    raise RuntimeError(f"failed to download tile {url}: {last_error}")


def is_complete_png(path: Path, size: tuple[int, int]) -> bool:
    if not path.exists():
        return False
    try:
        with Image.open(path) as image:
            return image.size == size
    except Exception:
        return False


def build_map(
    wiki_map: WikiMap,
    output_dir: Path,
    zoom: int,
    workers: int,
    retries: int,
    overwrite: bool,
) -> None:
    if zoom < 3 or zoom > 5:
        raise ValueError("full map zoom must satisfy 3 <= zoom <= 5")

    size = expected_size(zoom)
    tile_count = 2**zoom
    output_path = output_dir / f"{wiki_map.key}.png"
    temp_path = output_path.with_suffix(".png.tmp")

    if not overwrite and is_complete_png(output_path, size):
        print(f"SKIP {wiki_map.key}: existing {size[0]}x{size[1]}")
        return

    output_dir.mkdir(parents=True, exist_ok=True)
    if temp_path.exists():
        temp_path.unlink()

    print(
        f"START {wiki_map.key}: wiki_id={wiki_map.wiki_id}, "
        f"zoom={zoom}, tiles={tile_count * tile_count}, output={size[0]}x{size[1]}"
    )

    canvas = Image.new("RGBA", size)
    tiles = [(x, y) for y in range(tile_count) for x in range(tile_count)]
    completed = 0

    with requests.Session() as session:
        with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
            futures = [
                executor.submit(
                    fetch_tile,
                    session,
                    wiki_map.wiki_id,
                    None,
                    zoom,
                    x,
                    y,
                    retries,
                )
                for x, y in tiles
            ]

            for future in concurrent.futures.as_completed(futures):
                x, y, tile = future.result()
                canvas.paste(tile, (x * TILE_SIZE, y * TILE_SIZE))
                completed += 1
                if completed % 128 == 0 or completed == len(tiles):
                    print(f"  {wiki_map.key}: {completed}/{len(tiles)}")

    canvas.save(temp_path, format="PNG")
    temp_path.replace(output_path)
    print(f"DONE {wiki_map.key}: {output_path} ({output_path.stat().st_size} bytes)")


def is_complete_tile(path: Path) -> bool:
    if not path.exists():
        return False
    try:
        with Image.open(path) as image:
            return image.size == (TILE_SIZE, TILE_SIZE)
    except Exception:
        return False


def runtime_layers(wiki_map: WikiMap, requested: str) -> list[str]:
    if requested == "all":
        return ["surface", "underground"] if wiki_map.wiki_id == 6 else ["surface"]
    if requested == "underground" and wiki_map.wiki_id != 6:
        return []
    return [requested]


def build_runtime_tiles(
    wiki_map: WikiMap,
    output_dir: Path,
    min_zoom: int,
    max_zoom: int,
    layers: str,
    workers: int,
    retries: int,
    overwrite: bool,
) -> None:
    if min_zoom < 3 or max_zoom > 5 or min_zoom > max_zoom:
        raise ValueError("runtime zoom range must satisfy 3 <= min_zoom <= max_zoom <= 5")

    for layer in runtime_layers(wiki_map, layers):
        for zoom in range(min_zoom, max_zoom + 1):
            tile_count = 2**zoom
            layer_dir = output_dir / str(wiki_map.wiki_id) / layer / str(zoom)
            layer_dir.mkdir(parents=True, exist_ok=True)
            tiles = [(x, y) for y in range(tile_count) for x in range(tile_count)]
            pending = [
                (x, y)
                for x, y in tiles
                if overwrite or not is_complete_tile(layer_dir / str(x) / f"{y}.jpg")
            ]

            if not pending:
                print(f"SKIP tiles {wiki_map.key}/{layer}/z{zoom}: complete ({len(tiles)} tiles)")
                continue

            print(
                f"START tiles {wiki_map.key}/{layer}/z{zoom}: "
                f"pending={len(pending)}/{len(tiles)} workers={workers}"
            )

            completed = 0
            with requests.Session() as session:
                with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
                    futures = [
                        executor.submit(
                            fetch_tile,
                            session,
                            wiki_map.wiki_id,
                            None if layer == "surface" else layer,
                            zoom,
                            x,
                            y,
                            retries,
                        )
                        for x, y in pending
                    ]

                    for future in concurrent.futures.as_completed(futures):
                        x, y, tile = future.result()
                        tile_dir = layer_dir / str(x)
                        tile_dir.mkdir(parents=True, exist_ok=True)
                        output_path = tile_dir / f"{y}.jpg"
                        temp_path = output_path.with_suffix(".jpg.tmp")
                        tile.save(temp_path, format="JPEG", quality=95, subsampling=0, optimize=True)
                        temp_path.replace(output_path)
                        completed += 1
                        if completed % 64 == 0 or completed == len(pending):
                            print(f"  {wiki_map.key}/{layer}/z{zoom}: {completed}/{len(pending)}")

            print(f"DONE tiles {wiki_map.key}/{layer}/z{zoom}: {layer_dir}")


def main() -> int:
    args = parse_args()
    selected = {m.key: m for m in MAPS}
    if args.runtime_tiles:
        output_dir = Path(args.tile_output)
        for map_key in args.maps:
            build_runtime_tiles(
                selected[map_key],
                output_dir,
                args.min_zoom,
                args.max_zoom,
                args.layers,
                args.workers,
                args.retries,
                args.overwrite,
            )
        return 0

    output_dir = Path(args.output)

    for map_key in args.maps:
        build_map(
            selected[map_key],
            output_dir,
            args.zoom,
            args.workers,
            args.retries,
            args.overwrite,
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
