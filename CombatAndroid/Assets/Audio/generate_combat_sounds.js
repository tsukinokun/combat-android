#!/usr/bin/env node
//--------------------------------------------------------------
// generate_combat_sounds.js
// 戦闘中の効果音を、実際の音に寄せてプロシージャル生成し、同じディレクトリへ書き出す。
//
//   node generate_combat_sounds.js
//
// レトロ調（矩形波・大きく下がるピッチスイープ・はっきりした音程）は使わない。
// 音の芯は「帯域を絞ったノイズ」で作り、音程を持つのは数周期で消える低い芯（体に来る重さ）だけにする。
// 歪みは音程を持つ成分に掛けない（サインを歪ませると矩形波＝チップチューンの音になる）。
//
// - HitImpactBlunt.wav … 鈍器（ウォーハンマー）が当たる「ドゴッ」
// - HitImpactSharp.wav … 刃物（グレートソード/バトルアックス）が斬る「ザシュッ」
// - PlayerHurt.wav     … 被弾。こもった打撃と服の擦れ
// - EnemyDown.wav      … 撃破。体が地面へ倒れる「ドサッ」
// - Swing.wav          … 重い武器の振り「ブンッ」
// - Dodge.wav          … 回避。体が空を切る音と服の擦れ
// - ChargeFire.wav     … 溜めた斬撃弾が飛び出す「ドォン」
//
// UI・演出の音（メニュー・レベルアップ等）は generate_game_sounds.py が作る。
// Node 標準のみで動く。音を変えたいときは各関数の数値をいじって再実行する。
//--------------------------------------------------------------
'use strict';
const fs = require('fs');
const path = require('path');

const SR = 44100;

//--------------------------------------------------------------
// 乱数（線形合同法。音ごとに種を決めて毎回同じ結果にする）
//--------------------------------------------------------------
function makeRng(seed) {
    let state = seed >>> 0;
    return () => {
        state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
        return state / 2147483648.0 - 1.0;
    };
}

//--------------------------------------------------------------
// 2次のフィルター（RBJ の biquad）。中心周波数を動かせるよう係数は呼ぶたびに作れる
//--------------------------------------------------------------
class Biquad {
    constructor(type, freq, q) {
        this.type = type;
        this.x1 = this.x2 = this.y1 = this.y2 = 0;
        this.set(freq, q);
    }

    set(freq, q) {
        const w0 = 2 * Math.PI * Math.min(Math.max(freq, 10), SR * 0.45) / SR;
        const cos = Math.cos(w0);
        const alpha = Math.sin(w0) / (2 * q);
        let b0, b1, b2;
        if (this.type === 'lowpass') {
            b0 = (1 - cos) / 2; b1 = 1 - cos; b2 = (1 - cos) / 2;
        } else if (this.type === 'highpass') {
            b0 = (1 + cos) / 2; b1 = -(1 + cos); b2 = (1 + cos) / 2;
        } else {    // bandpass（ピークが0dBになる形）
            b0 = alpha; b1 = 0; b2 = -alpha;
        }
        const a0 = 1 + alpha;
        this.b0 = b0 / a0; this.b1 = b1 / a0; this.b2 = b2 / a0;
        this.a1 = (-2 * cos) / a0; this.a2 = (1 - alpha) / a0;
    }

    process(x) {
        const y = this.b0 * x + this.b1 * this.x1 + this.b2 * this.x2 - this.a1 * this.y1 - this.a2 * this.y2;
        this.x2 = this.x1; this.x1 = x;
        this.y2 = this.y1; this.y1 = y;
        return y;
    }
}

//--------------------------------------------------------------
// 部品
//--------------------------------------------------------------
const buffer = (sec) => new Float64Array(Math.floor(SR * sec));

// 立ち上がり attack 秒で1へ、その後 exp(-decay t) で減衰
const env = (t, attack, decay) => Math.min(t / Math.max(attack, 1e-5), 1) * Math.exp(-t * decay);

//! 帯域を絞ったノイズを足す。freq は数値か t→Hz の関数（フィルターの掃引は音程にならない）
//! grain>0 なら grain 秒ごとに強さをランダムに揺らし、ざらつき（砕ける・擦れる質感）を出す
function addNoise(out, rng, { start = 0, dur, type = 'bandpass', freq, q = 0.8, gain = 1, attack = 0.001, decay = 20,
    grain = 0, grainDepth = 0.6, shape = null }) {
    const f = new Biquad(type, typeof freq === 'function' ? freq(0) : freq, q);
    const begin = Math.floor(start * SR);
    const count = Math.floor(dur * SR);
    const grainLen = Math.max(Math.floor(grain * SR), 1);
    let g = 1;
    for (let i = 0; i < count && begin + i < out.length; i++) {
        const t = i / SR;
        if (typeof freq === 'function' && i % 32 === 0)
            f.set(freq(t), q);
        if (grain > 0 && i % grainLen === 0)
            g = 1 - grainDepth * (rng() * 0.5 + 0.5);
        const e = shape ? shape(t / dur) : env(t, attack, decay);
        out[begin + i] += f.process(rng()) * gain * g * e;
    }
}

//! 数周期で消える低い芯。音程として聞こえる前に消えるので「ドッ」という重さだけが残る
function addThump(out, { start = 0, dur, from, to, sweep = 40, gain = 1, attack = 0.001, decay = 25 }) {
    const begin = Math.floor(start * SR);
    const count = Math.floor(dur * SR);
    let phase = 0;
    for (let i = 0; i < count && begin + i < out.length; i++) {
        const t = i / SR;
        phase += ((from - to) * Math.exp(-t * sweep) + to) / SR;
        out[begin + i] += Math.sin(phase * 2 * Math.PI) * gain * env(t, attack, decay);
    }
}

//! ごく短い広帯域のクリック（硬いものが当たった瞬間）
function addClick(out, rng, { start = 0, gain = 0.5, highpass = 2500, decay = 700 }) {
    addNoise(out, rng, { start, dur: 0.02, type: 'highpass', freq: highpass, q: 0.7, gain, attack: 0.0002, decay });
}

//! 音程を持たない成分だけを軽く歪ませて密度を上げる
const saturate = (buf, drive) => {
    let peak = 0;
    for (const s of buf) peak = Math.max(peak, Math.abs(s));
    peak = peak || 1;
    for (let i = 0; i < buf.length; i++) buf[i] = Math.tanh(buf[i] / peak * drive) / Math.tanh(drive);
};

const mixInto = (dst, src, gain = 1) => { for (let i = 0; i < dst.length; i++) dst[i] += src[i] * gain; };

//! 小さな部屋の残響（櫛形4本＋全域通過2本）。遅延は互いに素に近い値にして、特定の音程で鳴かせない
function addRoom(buf, mix, size = 1.0, damp = 3000) {
    const combs = [0.0297, 0.0371, 0.0411, 0.0437].map((d) => Math.floor(d * size * SR));
    const wet = new Float64Array(buf.length);
    for (const d of combs) {
        const line = new Float64Array(buf.length);
        const lp = new Biquad('lowpass', damp, 0.707);
        for (let i = 0; i < buf.length; i++) {
            const fb = i >= d ? line[i - d] : 0;
            line[i] = buf[i] + lp.process(fb) * 0.62;
            wet[i] += line[i] * 0.25;
        }
    }
    for (const d of [0.005, 0.0017].map((s) => Math.floor(s * SR))) {
        const x = Float64Array.from(wet);
        for (let i = 0; i < wet.length; i++) {
            const delayed = i >= d ? wet[i - d] : 0;
            const xd = i >= d ? x[i - d] : 0;
            wet[i] = -0.5 * x[i] + xd + 0.5 * delayed;
        }
    }
    // 残響の頭（直接音と重なる部分）を少し遅らせ気味に効かせるため、立ち上がりを丸める
    for (let i = 0; i < buf.length; i++) buf[i] += wet[i] * mix * Math.min(i / (0.01 * SR), 1);
}

function finish(buf, peak = 0.9, fadeSec = 0.04) {
    // 直流を切る（低い芯の非対称で底上げされたぶん）
    const hp = new Biquad('highpass', 25, 0.707);
    for (let i = 0; i < buf.length; i++) buf[i] = hp.process(buf[i]);
    const fade = Math.floor(fadeSec * SR);
    for (let i = 0; i < fade; i++) buf[buf.length - 1 - i] *= i / fade;
    let m = 0;
    for (const s of buf) m = Math.max(m, Math.abs(s));
    m = m || 1;
    for (let i = 0; i < buf.length; i++) buf[i] = buf[i] / m * peak;
    return buf;
}

//--------------------------------------------------------------
// 効果音の定義
//--------------------------------------------------------------

//! ハンマー：硬い当たり＋重い芯＋肉と骨が潰れるざらつき
function hitImpactBlunt() {
    const rng = makeRng(20261011);
    const out = buffer(0.42);
    const body = buffer(0.42);
    addClick(body, rng, { gain: 0.6, highpass: 3000 });
    addNoise(body, rng, { dur: 0.3, freq: 380, q: 0.9, gain: 1.6, decay: 17 });                                       // 胴鳴り
    addNoise(body, rng, { dur: 0.12, freq: 1700, q: 1.1, gain: 0.9, decay: 32, grain: 0.004, grainDepth: 0.8 });     // 砕ける
    addNoise(body, rng, { dur: 0.35, type: 'lowpass', freq: 220, q: 0.7, gain: 1.4, decay: 14 });                     // 低い圧
    saturate(body, 1.6);
    mixInto(out, body);
    addThump(out, { dur: 0.25, from: 75, to: 48, sweep: 30, gain: 0.9, decay: 24 });
    addRoom(out, 0.18);
    return finish(out);
}

//! 刃物：肉を裂く高めの「シュッ」と湿った「ザッ」、軽い芯
function hitImpactSharp() {
    const rng = makeRng(20261012);
    const out = buffer(0.32);
    addClick(out, rng, { gain: 0.35, highpass: 4500, decay: 900 });
    addNoise(out, rng, { dur: 0.16, freq: (t) => 4200 * Math.exp(-t * 9) + 1500, q: 1.0, gain: 1.3, attack: 0.002, decay: 24 });    // 裂ける
    addNoise(out, rng, { dur: 0.14, freq: 650, q: 1.2, gain: 1.0, decay: 30, grain: 0.005, grainDepth: 0.85 });                       // 湿り
    addThump(out, { dur: 0.12, from: 110, to: 70, gain: 0.35, decay: 40 });
    addRoom(out, 0.14);
    return finish(out, 0.85);
}

//! 被弾：自分の体に響くこもった打撃と服の擦れ
function playerHurt() {
    const rng = makeRng(20261013);
    const out = buffer(0.38);
    addNoise(out, rng, { dur: 0.3, type: 'lowpass', freq: 700, q: 0.7, gain: 1.5, decay: 16 });
    addNoise(out, rng, { dur: 0.2, freq: 260, q: 1.0, gain: 1.0, decay: 18 });
    addNoise(out, rng, { start: 0.01, dur: 0.15, type: 'highpass', freq: 3000, q: 0.7, gain: 0.25, decay: 26, grain: 0.006, grainDepth: 0.9 });
    addThump(out, { dur: 0.25, from: 70, to: 45, gain: 0.9, decay: 20 });
    addRoom(out, 0.12);
    return finish(out, 0.85);
}

//! 撃破：体が地面へ倒れる「ドサッ」と、小さく跳ねたもう一度、土の散り
function enemyDown() {
    const rng = makeRng(20261014);
    const out = buffer(0.65);
    addNoise(out, rng, { dur: 0.35, type: 'lowpass', freq: 320, q: 0.8, gain: 1.6, attack: 0.004, decay: 13 });
    addThump(out, { dur: 0.3, from: 60, to: 40, gain: 0.8, attack: 0.003, decay: 16 });
    addNoise(out, rng, { start: 0.13, dur: 0.25, type: 'lowpass', freq: 420, q: 0.8, gain: 0.7, attack: 0.003, decay: 18 });
    addThump(out, { start: 0.13, dur: 0.2, from: 65, to: 45, gain: 0.35, decay: 22 });
    addNoise(out, rng, { start: 0.02, dur: 0.45, freq: 2400, q: 0.8, gain: 0.25, decay: 7, grain: 0.008, grainDepth: 0.95 });    // 土・砂利
    addRoom(out, 0.16);
    return finish(out, 0.8);
}

//! 重い武器の振り：帯域が上がって下がる風切り（近づいて離れる）
function swing() {
    const rng = makeRng(20261015);
    const out = buffer(0.32);
    const peakAt = 0.4;
    const shape = (p) => (p < peakAt ? Math.pow(p / peakAt, 1.6) : Math.pow((1 - p) / (1 - peakAt), 1.3));
    addNoise(out, rng, { dur: 0.3, freq: (t) => 350 + 900 * Math.sin(Math.min(t / 0.3, 1) * Math.PI), q: 1.4, gain: 1.0, shape });
    addNoise(out, rng, { dur: 0.3, type: 'lowpass', freq: 260, q: 0.7, gain: 0.6, shape });
    return finish(out, 0.6, 0.02);
}

//! 回避：体が空を切る短い風切りと、服の擦れ
function dodge() {
    const rng = makeRng(20261016);
    const out = buffer(0.26);
    const shape = (p) => Math.sin(Math.PI * Math.pow(p, 0.7));
    addNoise(out, rng, { dur: 0.22, freq: (t) => 700 + 1800 * Math.min(t / 0.22, 1), q: 1.0, gain: 0.9, shape });
    addNoise(out, rng, { dur: 0.22, type: 'highpass', freq: 2500, q: 0.7, gain: 0.35, shape, grain: 0.007, grainDepth: 0.9 });
    return finish(out, 0.6, 0.02);
}

//! 溜め解放：空気を押し出す「ドォン」と、飛び出す斬撃の風切り
function chargeFire() {
    const rng = makeRng(20261017);
    const out = buffer(0.75);
    addClick(out, rng, { gain: 0.5, highpass: 2000, decay: 500 });
    addNoise(out, rng, { dur: 0.6, type: 'lowpass', freq: 380, q: 0.8, gain: 1.8, attack: 0.002, decay: 7 });            // 押し出した空気の轟き
    addThump(out, { dur: 0.6, from: 62, to: 36, sweep: 12, gain: 1.0, attack: 0.002, decay: 7 });
    addNoise(out, rng, { dur: 0.4, freq: (t) => 2600 * Math.exp(-t * 5) + 500, q: 1.1, gain: 0.7, attack: 0.01, decay: 6 });    // 飛んでいく風切り
    addRoom(out, 0.24, 1.3);
    return finish(out, 0.9, 0.08);
}

//--------------------------------------------------------------
// 書き出し
//--------------------------------------------------------------
const OUTPUTS = [
    ['HitImpactBlunt.wav', hitImpactBlunt],
    ['HitImpactSharp.wav', hitImpactSharp],
    ['PlayerHurt.wav', playerHurt],
    ['EnemyDown.wav', enemyDown],
    ['Swing.wav', swing],
    ['Dodge.wav', dodge],
    ['ChargeFire.wav', chargeFire],
];

function writeWav(file, samples) {
    const data = Buffer.alloc(samples.length * 2);
    samples.forEach((s, i) => data.writeInt16LE(Math.trunc(Math.max(-1, Math.min(1, s)) * 32767), i * 2));
    const h = Buffer.alloc(44);
    h.write('RIFF', 0); h.writeUInt32LE(36 + data.length, 4); h.write('WAVE', 8);
    h.write('fmt ', 12); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20); h.writeUInt16LE(1, 22);
    h.writeUInt32LE(SR, 24); h.writeUInt32LE(SR * 2, 28); h.writeUInt16LE(2, 32); h.writeUInt16LE(16, 34);
    h.write('data', 36); h.writeUInt32LE(data.length, 40);
    fs.writeFileSync(file, Buffer.concat([h, data]));
}

for (const [name, generate] of OUTPUTS) {
    const samples = generate();
    writeWav(path.join(__dirname, name), samples);
    console.log(`generated: ${name} (${(samples.length / SR).toFixed(3)}s)`);
}
