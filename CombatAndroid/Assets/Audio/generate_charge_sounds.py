#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#--------------------------------------------------------------
# generate_charge_sounds.py
# 溜め攻撃（バトルアックス）の効果音をプロシージャル生成し、
# 同じディレクトリへ書き出す。作りはgenerate_hit_sounds.pyと同じレトロ調。
#
# - ChargeFire.wav … 斬撃弾が飛び出す瞬間の「ドン」（低く重い一発）
#
# 溜めている間の音は、合成音では耳障りになりやすかったため入れていない
#
# 標準ライブラリのみで完結する（wave / struct / math / random）。
# 音を変えたいときは各生成関数の頭にある定数をいじって再実行するだけでよい。
#--------------------------------------------------------------
import math
import os
import random
import struct
import wave

SAMPLE_RATE = 44100


#--------------------------------------------------------------
# ChargeFire：斬撃弾が飛び出す瞬間の「ドン」
#
# 作りはHitImpactBlunt（鈍器のドスッ）と同じで、こちらはより低く・長く・
# 頭のノイズを強くして、溜めを撃ち出した重さを出している
#--------------------------------------------------------------
FIRE_DURATION_SEC = 0.45
FIRE_SWEEP_START_HZ = 190.0    # 撃った瞬間の高さ
FIRE_SWEEP_END_HZ = 40.0       # 減衰し切ったときの高さ（腹に来る低さ）
FIRE_SWEEP_DECAY = 12.0        # ピッチが落ちる速さ
FIRE_ATTACK_SEC = 0.003
FIRE_ENVELOPE_DECAY = 6.0      # HitImpactBlunt(9.0)よりゆるめ＝長く響く
FIRE_TONE_MIX = 0.8
FIRE_NOISE_MIX = 0.35
FIRE_NOISE_DECAY = 60.0        # 頭のノイズバースト（打撃感）
FIRE_SUB_HZ = 55.0             # 下支えのサブ。芯とは別に長く残す
FIRE_SUB_DECAY = 5.0
FIRE_SUB_MIX = 0.5
FIRE_NORMALIZED_PEAK = 0.9
FIRE_SEED = 20260916


def generate_charge_fire():
    random.seed(FIRE_SEED)
    sample_count = int(SAMPLE_RATE * FIRE_DURATION_SEC)
    samples = []

    for i in range(sample_count):
        t = i / SAMPLE_RATE

        attack = min(t / FIRE_ATTACK_SEC, 1.0) if FIRE_ATTACK_SEC > 0 else 1.0
        envelope = attack * math.exp(-t * FIRE_ENVELOPE_DECAY)

        # ピッチスイープする芯（sine＝丸く重い）
        freq = (FIRE_SWEEP_START_HZ - FIRE_SWEEP_END_HZ) * math.exp(-t * FIRE_SWEEP_DECAY) + FIRE_SWEEP_END_HZ
        tone = math.sin((t * freq) % 1.0 * 2.0 * math.pi)

        # 撃ち出した瞬間の空気（ごく短いノイズ）
        noise = (random.random() * 2.0 - 1.0) * math.exp(-t * FIRE_NOISE_DECAY)

        # 下支えのサブ。芯より長く残して「ドーン」の尾を作る
        sub = math.sin(t * FIRE_SUB_HZ * 2.0 * math.pi) * math.exp(-t * FIRE_SUB_DECAY) * attack

        samples.append((tone * FIRE_TONE_MIX + noise * FIRE_NOISE_MIX) * envelope + sub * FIRE_SUB_MIX)

    peak = max((abs(s) for s in samples), default=1.0) or 1.0

    return [s / peak * FIRE_NORMALIZED_PEAK for s in samples]


def write_wav(path, samples):
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)

    with wave.open(path, "w") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SAMPLE_RATE)
        f.writeframesraw(frames)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    outputs = [("ChargeFire.wav", generate_charge_fire)]

    for filename, generator in outputs:
        samples = generator()
        out_path = os.path.join(out_dir, filename)
        write_wav(out_path, samples)
        print(f"generated: {out_path} ({len(samples)} samples, {len(samples) / SAMPLE_RATE:.3f}s)")


if __name__ == "__main__":
    main()
