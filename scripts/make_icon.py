#!/usr/bin/env python3
"""基于 resources/icons/firefly.png 生成多尺寸 app.ico。

输出：resources/app.ico  （含 16/32/48/64/128/256 多尺寸）
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "resources" / "icons" / "firefly.png"
OUT = ROOT / "resources" / "app.ico"

SIZES = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]

src = Image.open(SRC).convert("RGBA")
print(f"源图: {SRC.name}  {src.size}  {src.mode}")

# Pillow 写 ICO 时，单张图会自动下采样到各指定尺寸；
# 这里先放大到 256 作为主图，保证大尺寸清晰。
master = src.resize((256, 256), Image.LANCZOS)
master.save(OUT, format="ICO", sizes=SIZES)

print(f"已生成: {OUT}")
for w, h in SIZES:
    print(f"  - {w}x{h}")
