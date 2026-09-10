#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#--------------------------------------------------------------
# generate_dirt_ground.py
# 地面（草の生えていない地肌）用の、タイル張り前提の土テクスチャを
# プロシージャル生成し、同じディレクトリへ DirtGround.bmp として書き出す。
#
# 標準ライブラリのみで完結する（struct / math / random）。
# 拡張子は .bmp（無圧縮24bit）。AssetManagerが他の画像と同じ経路で
# 自動的にインポートできる形式で、追加の依存ライブラリが要らないため選んだ。
#
# タイル張り（UVを繰り返して広い地面に敷く）が前提なので、継ぎ目が出ない
# ことが最優先。すべての模様を「整数周波数の周期関数」または「輪環距離
# （トーラス距離）」だけで作ることで、端をまたいでも値が連続するようにしている。
#--------------------------------------------------------------
import math
import random
import struct

SIZE = 256    # 正方形。3*SIZEが4の倍数になるサイズを選ぶとBMPの行パディングが不要で楽

SEED = 20260910

# 土の基準色（linear風のやや暗め〜中間のブラウン。sRGBとして書き出す）
BASE_COLOR = (118, 88, 58)
COLOR_VARIANCE = (30, 24, 16)    # ノイズで各チャンネルに足す最大の揺れ幅

SPECKLE_COUNT = 70          # 小石・土くれに見立てた暗い斑点の数
SPECKLE_RADIUS_RANGE = (2.5, 7.0)
SPECKLE_DARKEN = 55          # 斑点の中心での暗さ


def toroidal_delta(a, b, size):
    """a-bを輪環距離（タイルの継ぎ目をまたぐ最短差）で返す"""
    d = (a - b) % size
    if d > size / 2:
        d -= size
    return d


def make_octaves(count, max_freq, rng):
    """整数周波数の2D正弦波を複数用意する。整数周波数なので
    sin(2*pi*f*x/SIZE)はちょうどSIZE周期で閉じ、タイルの境界をまたいでも
    連続する（＝継ぎ目が出ない）"""
    octaves = []
    for i in range(count):
        fx = rng.randint(1, max_freq)
        fy = rng.randint(1, max_freq)
        phase = rng.uniform(0.0, math.tau)
        amplitude = 1.0 / (i + 1)
        octaves.append((fx, fy, phase, amplitude))
    return octaves


def sample_noise(octaves, x, y, size):
    total = 0.0
    for fx, fy, phase, amplitude in octaves:
        total += amplitude * math.sin(2.0 * math.pi * fx * x / size + 2.0 * math.pi * fy * y / size + phase)
    return total


def generate_pixels():
    rng = random.Random(SEED)

    # 明暗のムラ（土の乾き方・色ムラ）。周波数の違う正弦波を3層重ねる
    macro_octaves = make_octaves(4, 4, rng)     # 大きなムラ
    micro_octaves = make_octaves(3, 12, rng)    # 細かいザラつき

    # 正規化用に一度サンプルしてスケールを求める
    macro_peak = sum(1.0 / (i + 1) for i in range(len(macro_octaves)))
    micro_peak = sum(1.0 / (i + 1) for i in range(len(micro_octaves)))

    # 小石・土くれの斑点。位置は自由に置き、距離だけ輪環距離で測ることで
    # タイルの境界をまたぐ斑点も継ぎ目なく見える
    speckles = []
    for _ in range(SPECKLE_COUNT):
        cx = rng.uniform(0, SIZE)
        cy = rng.uniform(0, SIZE)
        radius = rng.uniform(*SPECKLE_RADIUS_RANGE)
        speckles.append((cx, cy, radius))

    pixels = [[None] * SIZE for _ in range(SIZE)]

    for y in range(SIZE):
        for x in range(SIZE):
            macro = sample_noise(macro_octaves, x, y, SIZE) / macro_peak    # -1..1
            micro = sample_noise(micro_octaves, x, y, SIZE) / micro_peak    # -1..1

            # マクロ(大きなムラ)を主、ミクロ(ザラつき)を弱めに足す
            shade = macro * 0.75 + micro * 0.25

            r = BASE_COLOR[0] + shade * COLOR_VARIANCE[0]
            g = BASE_COLOR[1] + shade * COLOR_VARIANCE[1]
            b = BASE_COLOR[2] + shade * COLOR_VARIANCE[2]

            # 斑点。輪環距離で最も近いものだけ効かせる
            darken = 0.0
            for cx, cy, radius in speckles:
                dx = toroidal_delta(x, cx, SIZE)
                dy = toroidal_delta(y, cy, SIZE)
                dist = math.sqrt(dx * dx + dy * dy)
                if dist < radius:
                    falloff = 1.0 - (dist / radius)
                    darken = max(darken, falloff)

            r -= SPECKLE_DARKEN * darken
            g -= SPECKLE_DARKEN * darken
            b -= SPECKLE_DARKEN * darken

            pixels[y][x] = (
                int(max(0, min(255, r))),
                int(max(0, min(255, g))),
                int(max(0, min(255, b))),
            )

    return pixels


def write_bmp(path, pixels):
    height = len(pixels)
    width = len(pixels[0])

    row_size = width * 3
    padding = (4 - (row_size % 4)) % 4
    image_size = (row_size + padding) * height

    file_header = struct.pack(
        "<2sIHHI",
        b"BM",
        14 + 40 + image_size,
        0, 0,
        14 + 40,
    )
    dib_header = struct.pack(
        "<IiiHHIIiiII",
        40,           # ヘッダサイズ
        width,
        height,       # 正の値=ボトムアップ格納
        1,            # プレーン数
        24,           # bitCount
        0,            # 無圧縮
        image_size,
        0, 0,         # 解像度（未使用）
        0, 0,         # パレット関連（未使用）
    )

    with open(path, "wb") as f:
        f.write(file_header)
        f.write(dib_header)

        # BMPは下から上、かつBGRの並びで書く
        for y in range(height - 1, -1, -1):
            row = bytearray()
            for x in range(width):
                r, g, b = pixels[y][x]
                row += bytes((b, g, r))
            row += b"\x00" * padding
            f.write(row)


def main():
    import os

    pixels = generate_pixels()
    out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "DirtGround.bmp")
    write_bmp(out_path, pixels)
    print(f"generated: {out_path} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
