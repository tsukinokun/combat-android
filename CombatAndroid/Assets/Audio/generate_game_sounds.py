#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#--------------------------------------------------------------
# generate_game_sounds.py
# ゲーム全体の効果音をプロシージャル生成し、同じディレクトリへ書き出す。
# 質感はgenerate_hit_sounds.pyと同じレトロ調。
#
# どれも0.5秒以下の短い音にしてある。長く鳴り続ける合成音は、うなりや
# 倍音の癖がそのまま耳障りさになりやすい（溜め音で一度作って没にしている）。
# 長い音（BGM）は合成せず、Assets/Audio/README.md のとおり実素材を置く。
#
# 鳴らす側の対応（どの場面でどれを鳴らすか）は
# CombatAndroid/ECS/Utility/SoundTable.cpp の表にある。
#
# 標準ライブラリのみで完結する（wave / struct / math / random）。
# 音を変えたいときは各定義の数値をいじって再実行するだけでよい。
#--------------------------------------------------------------
import math
import os
import random
import struct
import wave

SAMPLE_RATE = 44100


#--------------------------------------------------------------
# 音を組み立てる部品
#--------------------------------------------------------------
def tone(samples, start_sec, duration_sec, freq_start, freq_end, volume, waveform="sine", decay=6.0, attack_sec=0.004):
    """減衰する音を1つ、samples（配列）へ足し込む。freqは指数で変化させる"""
    begin = int(start_sec * SAMPLE_RATE)
    count = int(duration_sec * SAMPLE_RATE)
    phase = 0.0

    for i in range(count):
        index = begin + i
        if index >= len(samples):
            break

        t = i / SAMPLE_RATE
        progress = t / duration_sec if duration_sec > 0.0 else 1.0

        freq = freq_start * math.pow(freq_end / freq_start, progress)
        phase = (phase + freq / SAMPLE_RATE) % 1.0

        if waveform == "square":
            value = 1.0 if phase < 0.5 else -1.0
        elif waveform == "triangle":
            value = 4.0 * abs(phase - 0.5) - 1.0
        else:
            value = math.sin(phase * 2.0 * math.pi)

        attack = min(t / attack_sec, 1.0) if attack_sec > 0.0 else 1.0
        samples[index] += value * volume * attack * math.exp(-t * decay)


def noise(samples, start_sec, duration_sec, volume, decay=30.0, low_pass=0.0):
    """ノイズを足し込む。low_passは0で素のノイズ、1に近いほど低くこもった風切りになる"""
    begin = int(start_sec * SAMPLE_RATE)
    count = int(duration_sec * SAMPLE_RATE)
    previous = 0.0

    for i in range(count):
        index = begin + i
        if index >= len(samples):
            break

        t = i / SAMPLE_RATE
        value = random.random() * 2.0 - 1.0

        # 1次のローパス。係数を上げるほど直前の値へ引きずられ、高い成分が落ちる
        previous = previous * low_pass + value * (1.0 - low_pass)

        samples[index] += previous * volume * math.exp(-t * decay)


def sweep_noise(samples, start_sec, duration_sec, volume, low_pass_start, low_pass_end):
    """風切り。ローパスの強さを変化させて「シュッ」と抜ける感じを出す"""
    begin = int(start_sec * SAMPLE_RATE)
    count = int(duration_sec * SAMPLE_RATE)
    previous = 0.0

    for i in range(count):
        index = begin + i
        if index >= len(samples):
            break

        t = i / SAMPLE_RATE
        progress = t / duration_sec if duration_sec > 0.0 else 1.0

        low_pass = low_pass_start + (low_pass_end - low_pass_start) * progress
        previous = previous * low_pass + (random.random() * 2.0 - 1.0) * (1.0 - low_pass)

        # 山なりの音量（無音から入って無音へ抜ける）
        envelope = math.sin(progress * math.pi)
        samples[index] += previous * volume * envelope


def make_buffer(duration_sec):
    return [0.0] * int(SAMPLE_RATE * duration_sec)


def normalize(samples, peak=0.85):
    highest = max((abs(s) for s in samples), default=1.0) or 1.0
    return [s / highest * peak for s in samples]


#--------------------------------------------------------------
# 効果音の定義
#--------------------------------------------------------------
def player_hurt():
    """被弾。低く潰れた打撃＋短いノイズ"""
    samples = make_buffer(0.32)
    tone(samples, 0.0, 0.30, 220.0, 70.0, 0.8, "triangle", decay=11.0)
    noise(samples, 0.0, 0.10, 0.5, decay=45.0, low_pass=0.4)
    return normalize(samples)


def enemy_down():
    """撃破。短く落ちるピッチ＋散るノイズ"""
    samples = make_buffer(0.30)
    tone(samples, 0.0, 0.26, 420.0, 90.0, 0.7, "square", decay=13.0)
    noise(samples, 0.0, 0.18, 0.35, decay=22.0, low_pass=0.55)
    return normalize(samples, 0.8)


def swing():
    """攻撃の振り。太めの風切り"""
    samples = make_buffer(0.26)
    sweep_noise(samples, 0.0, 0.24, 0.9, 0.90, 0.55)
    return normalize(samples, 0.55)


def dodge():
    """回避。攻撃より高く短い風切り"""
    samples = make_buffer(0.22)
    sweep_noise(samples, 0.0, 0.20, 0.9, 0.80, 0.30)
    return normalize(samples, 0.6)


def pickup():
    """武器の拾得。明るい2音"""
    samples = make_buffer(0.30)
    tone(samples, 0.00, 0.14, 880.0, 880.0, 0.6, "sine", decay=14.0)
    tone(samples, 0.08, 0.20, 1320.0, 1320.0, 0.6, "sine", decay=11.0)
    return normalize(samples, 0.7)


def weapon_level_up():
    """武器のレベルアップ。拾得より華やかな3音"""
    samples = make_buffer(0.42)
    tone(samples, 0.00, 0.16, 784.0, 784.0, 0.5, "sine", decay=13.0)
    tone(samples, 0.08, 0.16, 988.0, 988.0, 0.5, "sine", decay=13.0)
    tone(samples, 0.16, 0.26, 1319.0, 1319.0, 0.6, "sine", decay=9.0)
    return normalize(samples, 0.7)


def level_up():
    """プレイヤーのレベルアップ。和音的な3音（少し厚く）"""
    samples = make_buffer(0.48)
    for start, freq in ((0.00, 523.0), (0.09, 659.0), (0.18, 784.0)):
        tone(samples, start, 0.30, freq, freq, 0.45, "sine", decay=7.0)
        tone(samples, start, 0.30, freq * 2.0, freq * 2.0, 0.15, "sine", decay=9.0)
    return normalize(samples, 0.75)


def skill_pick():
    """スキル取得。確定感のある2音"""
    samples = make_buffer(0.34)
    tone(samples, 0.00, 0.14, 660.0, 660.0, 0.55, "triangle", decay=14.0)
    tone(samples, 0.09, 0.24, 990.0, 990.0, 0.55, "triangle", decay=10.0)
    return normalize(samples, 0.7)


def danger_up():
    """危険度上昇・ラスト1分。低い2音の警告"""
    samples = make_buffer(0.46)
    tone(samples, 0.00, 0.22, 196.0, 196.0, 0.6, "square", decay=9.0)
    tone(samples, 0.16, 0.28, 147.0, 147.0, 0.6, "square", decay=7.0)
    return normalize(samples, 0.7)


def menu_move():
    """カーソル移動。ごく短いクリック"""
    samples = make_buffer(0.07)
    tone(samples, 0.0, 0.06, 1200.0, 900.0, 0.5, "square", decay=60.0, attack_sec=0.001)
    return normalize(samples, 0.45)


def menu_confirm():
    """決定。短い確定音"""
    samples = make_buffer(0.20)
    tone(samples, 0.00, 0.08, 880.0, 880.0, 0.5, "square", decay=28.0, attack_sec=0.001)
    tone(samples, 0.05, 0.14, 1320.0, 1320.0, 0.5, "square", decay=20.0, attack_sec=0.001)
    return normalize(samples, 0.55)


def run_clear():
    """クリア。上昇する4音"""
    samples = make_buffer(0.90)
    for i, freq in enumerate((523.0, 659.0, 784.0, 1047.0)):
        tone(samples, 0.12 * i, 0.45, freq, freq, 0.5, "sine", decay=5.0)
        tone(samples, 0.12 * i, 0.45, freq * 2.0, freq * 2.0, 0.12, "sine", decay=7.0)
    return normalize(samples, 0.8)


def run_failed():
    """ゲームオーバー。下降する3音"""
    samples = make_buffer(0.95)
    for i, freq in enumerate((392.0, 311.0, 233.0)):
        tone(samples, 0.20 * i, 0.50, freq, freq, 0.55, "triangle", decay=4.5)
    return normalize(samples, 0.8)


#! 生成するファイルと中身。音を1つ足すときはここへ1行足し、SoundTable.cppの表にも足す
OUTPUTS = (
    ("PlayerHurt.wav", player_hurt),
    ("EnemyDown.wav", enemy_down),
    ("Swing.wav", swing),
    ("Dodge.wav", dodge),
    ("Pickup.wav", pickup),
    ("WeaponLevelUp.wav", weapon_level_up),
    ("LevelUp.wav", level_up),
    ("SkillPick.wav", skill_pick),
    ("DangerUp.wav", danger_up),
    ("MenuMove.wav", menu_move),
    ("MenuConfirm.wav", menu_confirm),
    ("RunClear.wav", run_clear),
    ("RunFailed.wav", run_failed),
)


def write_wav(path, samples):
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)

    with wave.open(path, "w") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SAMPLE_RATE)
        f.writeframesraw(frames)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    for filename, generator in OUTPUTS:
        random.seed(20260918)    # ノイズを含む音も毎回同じ結果になるようにする
        samples = generator()
        out_path = os.path.join(out_dir, filename)
        write_wav(out_path, samples)
        print(f"generated: {filename} ({len(samples) / SAMPLE_RATE:.3f}s)")


if __name__ == "__main__":
    main()
