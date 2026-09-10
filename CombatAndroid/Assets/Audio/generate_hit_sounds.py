#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#--------------------------------------------------------------
# generate_hit_sounds.py
# 攻撃ヒット音（レトロ・チップチューン風）を武器の質感ごとに
# プロシージャル生成し、同じディレクトリへ書き出す。
#
# - HitImpactBlunt.wav … 鈍器（ウォーハンマー）向けの重い「ドスッ」
# - HitImpactSharp.wav … 刃物（グレートソード/バトルアックス）向けの鋭い「キンッ」
#
# 標準ライブラリのみで完結する（wave / struct / math / random）。
# 音を変えたいときはPRESETSの値をいじって再実行するだけでよい。
#--------------------------------------------------------------
import math
import os
import random
import struct
import wave

SAMPLE_RATE = 44100


class Preset:
    def __init__(self, *, duration_sec, sweep_start_hz, sweep_end_hz, sweep_decay, waveform,
                 attack_sec, envelope_decay, square_mix, noise_mix, noise_decay,
                 ring_hz=None, ring_decay=0.0, ring_mix=0.0, normalized_peak=0.85, seed=20260910):
        self.duration_sec = duration_sec
        self.sweep_start_hz = sweep_start_hz
        self.sweep_end_hz = sweep_end_hz
        self.sweep_decay = sweep_decay
        self.waveform = waveform    # "square" or "sine"
        self.attack_sec = attack_sec
        self.envelope_decay = envelope_decay
        self.square_mix = square_mix
        self.noise_mix = noise_mix
        self.noise_decay = noise_decay
        self.ring_hz = ring_hz            # 追加の金属的な「キーン」を鳴らす場合の周波数
        self.ring_decay = ring_decay
        self.ring_mix = ring_mix
        self.normalized_peak = normalized_peak
        self.seed = seed


PRESETS = {
    # 鈍器：低い周波数・丸い波形（sine）・ゆっくり減衰させて「ドスッ」という重さを出す。
    # ノイズはごく短く弱め（衝突の瞬間の空気感だけ）で、鋭さを持たせない
    "HitImpactBlunt.wav": Preset(
        duration_sec=0.30,
        sweep_start_hz=260.0,
        sweep_end_hz=70.0,
        sweep_decay=10.0,
        waveform="sine",
        attack_sec=0.004,
        envelope_decay=9.0,
        square_mix=0.75,
        noise_mix=0.25,
        noise_decay=140.0,
        normalized_peak=0.85,
    ),
    # 刃物：高い周波数・矩形波（倍音が多く硬い）・速い減衰で「キンッ」という鋭さを出す。
    # ノイズを強めに混ぜ、さらに金属的な高音のリング（減衰する純音）を足して質感を足す
    "HitImpactSharp.wav": Preset(
        duration_sec=0.12,
        sweep_start_hz=1500.0,
        sweep_end_hz=500.0,
        sweep_decay=30.0,
        waveform="square",
        attack_sec=0.001,
        envelope_decay=40.0,
        square_mix=0.55,
        noise_mix=0.8,
        noise_decay=70.0,
        ring_hz=3200.0,
        ring_decay=35.0,
        ring_mix=0.35,
        normalized_peak=0.85,
    ),
}


def generate_samples(preset: Preset):
    random.seed(preset.seed)
    sample_count = int(SAMPLE_RATE * preset.duration_sec)
    samples = []

    for i in range(sample_count):
        t = i / SAMPLE_RATE

        # エンベロープ：立ち上がり→急減衰
        attack = min(t / preset.attack_sec, 1.0) if preset.attack_sec > 0 else 1.0
        decay = math.exp(-t * preset.envelope_decay)
        envelope = attack * decay

        # ピッチスイープする芯の波形
        freq = (preset.sweep_start_hz - preset.sweep_end_hz) * math.exp(-t * preset.sweep_decay) + preset.sweep_end_hz
        phase = (t * freq) % 1.0
        if preset.waveform == "square":
            tone = 1.0 if phase < 0.5 else -1.0
        else:
            tone = math.sin(phase * 2.0 * math.pi)

        # 衝突の瞬間のノイズバースト
        noise = (random.random() * 2.0 - 1.0) * math.exp(-t * preset.noise_decay)

        value = (tone * preset.square_mix + noise * preset.noise_mix) * envelope

        # 金属的なリング（刃物のみ）
        if preset.ring_hz:
            ring = math.sin(t * preset.ring_hz * 2.0 * math.pi) * math.exp(-t * preset.ring_decay)
            value += ring * preset.ring_mix * attack

        samples.append(value)

    # ピーク正規化してクリップを避ける
    peak = max((abs(s) for s in samples), default=1.0) or 1.0
    samples = [s / peak * preset.normalized_peak for s in samples]

    return samples


def write_wav(path, samples):
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)

    with wave.open(path, "w") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SAMPLE_RATE)
        f.writeframesraw(frames)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    for filename, preset in PRESETS.items():
        samples = generate_samples(preset)
        out_path = os.path.join(out_dir, filename)
        write_wav(out_path, samples)
        print(f"generated: {out_path} ({len(samples)} samples, {preset.duration_sec:.3f}s)")


if __name__ == "__main__":
    main()
