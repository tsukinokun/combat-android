#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#--------------------------------------------------------------
# generate_skill_art.py
# スキル（七つの大罪）のカード背景とHUDアイコンをプロシージャル生成し、
# 同じディレクトリへ書き出す。
#
#   <Skill>Card.png … 340x460。スキル選択メニューの縦長カード背景（左上と右下の角を斜めに落とす）
#   <Skill>Icon.png … 64x64。画面左上の取得済みスキル一覧のアイコン
#
# どちらも「白〜灰のグレースケール」で描く。スキルごとの色は
# SkillTable.cpp の panelColor が乗算で乗るので、ここで色を持つと二重に掛かる。
#
# 紋章は単純な幾何学図形の組み合わせ。ピクセルごとに「図形の内側か」を
# 2x2のサブサンプルで判定して、輪郭のギザギザを均している。
#
# 標準ライブラリのみで完結する（zlib / struct / math）。
# 図形を変えたいときは各 emblem_* 関数を書き換えて再実行するだけでよい。
#--------------------------------------------------------------
import math
import os
import struct
import zlib

CARD_WIDTH = 340
CARD_HEIGHT = 460
ICON_SIZE = 64

#! カードの明るさ。上端（紋章側）を明るく、下（文字が載る側）へ向かって暗くする
CARD_TOP_LEVEL = 0.62
CARD_BOTTOM_LEVEL = 0.16

#! 左上と右下の角を斜めに落とす量（ピクセル）。四角いカードが並ぶ単調さを崩す
CARD_CHAMFER = 44

#! 内側の細い縁取り（カードの縁からの距離と太さ、明るさ）
CARD_INNER_LINE_INSET = 10
CARD_INNER_LINE_WIDTH = 2
CARD_INNER_LINE_LEVEL = 0.85

#! カード内での紋章の位置と大きさ（ピクセル）。
#! 下半分はスキル名・段階・説明文が載るので、上側の空いたところへ置く
CARD_EMBLEM_CENTER_X = CARD_WIDTH // 2
CARD_EMBLEM_CENTER_Y = 140
CARD_EMBLEM_RADIUS = 78

#! 紋章の明るさ（カードの地より明るくして浮かせる）
EMBLEM_LEVEL = 1.0


#--------------------------------------------------------------
# 図形の判定。いずれも中心(0,0)・半径1の正規化した座標で「内側か」を返す
#--------------------------------------------------------------
def emblem_greed(x, y):
    """強欲：硬貨の輪（太めのリング）"""
    r = math.hypot(x, y)
    return 0.55 < r < 0.95


def emblem_gluttony(x, y):
    """暴食：一口ぶん欠けた円"""
    if math.hypot(x, y) > 0.92:
        return False
    # 右上を円で削る＝噛み跡
    return math.hypot(x - 0.72, y + 0.72) > 0.72


def emblem_wrath(x, y):
    """憤怒：上向きの三角（炎の形）"""
    if y > 0.85 or y < -0.95:
        return False
    # 上へ行くほど幅が狭い
    half_width = (y + 0.95) / 1.8 * 0.95
    return abs(x) < half_width


def emblem_pride(x, y):
    """傲慢：冠（3つの山と台座）"""
    if 0.35 < y < 0.8:
        return abs(x) < 0.9    # 台座
    if -0.85 < y <= 0.35:
        # 3つの山。各山は上へ向かって細くなる三角
        for center in (-0.58, 0.0, 0.58):
            peak = -0.85 if center == 0.0 else -0.55
            if y < peak:
                continue
            half_width = (y - peak) / (0.35 - peak) * 0.28
            if abs(x - center) < half_width:
                return True
    return False


def emblem_envy(x, y):
    """嫉妬：眼（レンズ形のまぶたの輪郭と瞳）"""
    outer = (x * x) / 0.98 + (y * y) / 0.32 < 1.0
    inner = (x * x) / 0.66 + (y * y) / 0.16 < 1.0

    if outer and not inner:
        return True    # まぶたの輪郭

    return math.hypot(x, y) < 0.26    # 瞳


def emblem_lust(x, y):
    """色欲：ハート（2つの円＋下向きの三角）"""
    if math.hypot(x + 0.38, y + 0.34) < 0.46 or math.hypot(x - 0.38, y + 0.34) < 0.46:
        return True
    if y < -0.32:
        return False
    return abs(x) < (0.92 - y * 0.92) and y < 0.92


def emblem_sloth(x, y):
    """怠惰：三日月"""
    if math.hypot(x, y) > 0.92:
        return False
    return math.hypot(x - 0.42, y - 0.10) > 0.70


#! スキルごとの紋章。ファイル名の頭とSkillTable.cppの並びに合わせてある
EMBLEMS = (
    ("Greed", emblem_greed),
    ("Gluttony", emblem_gluttony),
    ("Wrath", emblem_wrath),
    ("Pride", emblem_pride),
    ("Envy", emblem_envy),
    ("Lust", emblem_lust),
    ("Sloth", emblem_sloth),
)


def emblem_coverage(shape, px, py, center_x, center_y, radius):
    """ピクセル(px, py)が紋章に覆われている割合（0〜1）。2x2のサブサンプルで均す"""
    covered = 0
    for offset_y in (0.25, 0.75):
        for offset_x in (0.25, 0.75):
            nx = (px + offset_x - center_x) / radius
            ny = (py + offset_y - center_y) / radius
            if abs(nx) <= 1.2 and abs(ny) <= 1.2 and shape(nx, ny):
                covered += 1

    return covered / 4.0


def chamfer_distance(x, y, inset):
    """角を落としたカードの輪郭から内側へどれだけ入っているか（ピクセル。負なら外）"""
    left = x - inset
    right = (CARD_WIDTH - 1 - inset) - x
    top = y - inset
    bottom = (CARD_HEIGHT - 1 - inset) - y
    # 斜めの辺（左上・右下）。45度の線からの距離
    top_left = (x + y - (CARD_CHAMFER + inset * 1.414)) / 1.414
    bottom_right = ((CARD_WIDTH - 1 + CARD_HEIGHT - 1) - (CARD_CHAMFER + inset * 1.414) - (x + y)) / 1.414
    return min(left, right, top, bottom, top_left, bottom_right)


def make_card(shape):
    """カード背景（縦長）。上から下へ暗くなる地の上に紋章を置き、角を斜めに落とす"""
    pixels = []

    for y in range(CARD_HEIGHT):
        row = []
        for x in range(CARD_WIDTH):
            # 地：縦方向のグラデーション
            t = y / (CARD_HEIGHT - 1)
            level = CARD_TOP_LEVEL + (CARD_BOTTOM_LEVEL - CARD_TOP_LEVEL) * t

            inside = chamfer_distance(x, y, 0)

            # 縁へ近いほど少し暗くして、カードの輪郭を締める
            level *= 0.6 + 0.4 * min(max(inside, 0.0) / 14.0, 1.0)

            # 内側の細い縁取り
            line = chamfer_distance(x, y, CARD_INNER_LINE_INSET)
            if -CARD_INNER_LINE_WIDTH <= line <= 0.0:
                level = CARD_INNER_LINE_LEVEL

            # 紋章
            coverage = emblem_coverage(shape, x, y, CARD_EMBLEM_CENTER_X, CARD_EMBLEM_CENTER_Y, CARD_EMBLEM_RADIUS)
            level = level + (EMBLEM_LEVEL - level) * coverage

            # 輪郭の外は透明。境目の1ピクセルだけ半透明にしてギザギザを均す
            alpha = max(0.0, min(1.0, inside + 0.5))

            row.append((int(max(0.0, min(1.0, level)) * 255), int(alpha * 255)))
        pixels.append(row)

    return pixels


def make_icon(shape):
    """HUDアイコン（64x64）。暗い地に紋章だけ"""
    pixels = []
    radius = ICON_SIZE * 0.36

    for y in range(ICON_SIZE):
        row = []
        for x in range(ICON_SIZE):
            level = 0.22    # 地

            coverage = emblem_coverage(shape, x, y, ICON_SIZE * 0.5, ICON_SIZE * 0.5, radius)
            level = level + (EMBLEM_LEVEL - level) * coverage

            row.append(int(max(0.0, min(1.0, level)) * 255))
        pixels.append(row)

    return pixels


def write_png(path, pixels):
    """RGBA 8bitのPNGを書く。灰色の値をRGBへ入れる。(灰色, アルファ) の組ならアルファも書く"""
    height = len(pixels)
    width = len(pixels[0])

    # 各行の先頭にフィルタ種別（0＝フィルタなし）を置くのがPNGの生データ形式。
    # グレースケールのまま書くと読み込み側の対応に依存するので、
    # 他のUI画像（WhitePixel.png）と同じRGBAへ展開しておく
    def rgba(texel):
        level, alpha = texel if isinstance(texel, tuple) else (texel, 255)
        return (level, level, level, alpha)

    raw = b"".join(b"\x00" + bytes(value for texel in row for value in rgba(texel)) for row in pixels)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)    # 8bit / RGBA
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")

    with open(path, "wb") as f:
        f.write(png)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    for name, shape in EMBLEMS:
        card_path = os.path.join(out_dir, f"{name}Card.png")
        icon_path = os.path.join(out_dir, f"{name}Icon.png")

        write_png(card_path, make_card(shape))
        write_png(icon_path, make_icon(shape))

        print(f"generated: {name}Card.png ({CARD_WIDTH}x{CARD_HEIGHT}), {name}Icon.png ({ICON_SIZE}x{ICON_SIZE})")


if __name__ == "__main__":
    main()
