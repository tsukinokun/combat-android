#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#--------------------------------------------------------------
# generate_ui_shapes.py
# メニューの強調帯などに敷く「斜めに切った板」をプロシージャル生成し、
# 同じディレクトリへ書き出す。
#
#   SlashPlate.png … 512x128。左右の辺を斜めに切った平行四辺形（白、縁はアンチエイリアス）
#
# エンジンのスプライトは矩形しか描けない（スキューも任意ポリゴンも無い）ので、
# 「傾いた板」の形はテクスチャのアルファで作り、さらにスプライトを少し回転させて使う。
# 色は呼び出し側の tintColor を乗算するので、ここでは白で描く。
#
# 標準ライブラリのみで完結する（zlib / struct）。
#--------------------------------------------------------------
import os
import struct
import zlib

SLASH_WIDTH = 512
SLASH_HEIGHT = 128

#! 上辺が下辺に対して右へずれる量（ピクセル）。左右の辺の傾きになる
SLASH_SHEAR = 40


def slash_coverage(px, py):
    """ピクセル(px, py)が平行四辺形に覆われている割合（0〜1）。4x4のサブサンプルで均す"""
    covered = 0
    for sy in range(4):
        for sx in range(4):
            x = px + (sx + 0.5) / 4.0
            y = py + (sy + 0.5) / 4.0
            # 上（y=0）ほど右へずれる。下端で0、上端でSLASH_SHEAR
            shift = SLASH_SHEAR * (1.0 - y / SLASH_HEIGHT)
            if shift <= x <= SLASH_WIDTH - SLASH_SHEAR + shift:
                covered += 1
    return covered / 16.0


def make_slash():
    return [[(255, int(slash_coverage(x, y) * 255)) for x in range(SLASH_WIDTH)] for y in range(SLASH_HEIGHT)]


def write_png(path, pixels):
    """RGBA 8bitのPNGを書く。pixelsは (灰色の値, アルファ) の2次元配列"""
    height = len(pixels)
    width = len(pixels[0])

    raw = b"".join(b"\x00" + bytes(v for level, alpha in row for v in (level, level, level, alpha)) for row in pixels)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)    # 8bit / RGBA
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")

    with open(path, "wb") as f:
        f.write(png)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))
    write_png(os.path.join(out_dir, "SlashPlate.png"), make_slash())
    print(f"generated: SlashPlate.png ({SLASH_WIDTH}x{SLASH_HEIGHT})")


if __name__ == "__main__":
    main()
