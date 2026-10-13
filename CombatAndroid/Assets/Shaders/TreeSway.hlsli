//--------------------------------------------------------------
//! @file   TreeSway.hlsli
//! @brief  木を風で揺らすずれの計算（本体の頂点シェーダーと影の頂点シェーダーで共用）
//! @note   本体と影で同じ式を通すので、影も本体と一緒に揺れる。
//!         パラメータはTreeFieldSystemがゲーム予約枠(b12)へ書く。
//!         風向きと突風は草（Grass.vs.hlsl）と同じ値・同じ式にしてあり、
//!         突風の波が草と木を一緒に通り抜ける
//--------------------------------------------------------------
#ifndef COMBAT_TREE_SWAY_HLSLI
#define COMBAT_TREE_SWAY_HLSLI

//--------------------------------------------------------------
//! @brief 木の揺れのパラメータ (b12)
//! @note  TreeFieldSystem.hpp の CBufferTree と1バイト単位で一致させること
//--------------------------------------------------------------
cbuffer CBufferTree : register(b12)
{
    float4 windParams;    // xz: 風向き（水平・正規化済み）, yw: 予約
    float4 swayParams;    // x: 幹のしなりの強さ（基準の高さでの振れ幅）, y: しなりの速さ, z: 葉のそよぎの強さ（0で無し）, w: そよぎの速さ
    float4 gustParams;    // x: 突風の波長, y: 突風の速さ, z: 突風の強さ, w: しなりの基準の高さ
    float4 shakeSlots[8]; // 攻撃で揺れている木ごとに xy: 根元のXZ, zw: しなりのずれのXZ（基準の高さでの量）
    float4 shakeParams;   // x: 使っているスロット数, y: ずれ1あたりに葉のそよぎを強める量, zw: 予約
};

// 攻撃で揺らせる木の数。TreeFieldSystem.hpp の kMaxShakingTrees と一致させること
static const uint kMaxShakingTrees = 8;

static const float kTreeTwoPi = 6.28318531f;

//--------------------------------------------------------------
//! 根元の位置から、木ごとの位相（0〜2π）を作ります。
//! @param  [in] p 根元のXZ
//! @return 位相。全部の木が同じ拍子で揺れないようにするためのずらし
//--------------------------------------------------------------
float TreePhase(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f) * kTreeTwoPi;
}

//--------------------------------------------------------------
//! 風で揺らすずれを求めます。
//! @param  [in] worldPos 揺らす前の頂点のワールド座標
//! @param  [in] treeBase 木の根元のワールド座標（ワールド行列の平行移動）
//! @param  [in] time     経過秒
//! @return 頂点に足すずれ（ワールド座標）
//--------------------------------------------------------------
float3 TreeSwayOffset(float3 worldPos, float3 treeBase, float time)
{
    const float2 windDir = windParams.xz;
    const float3 windDir3 = float3(windDir.x, 0.0f, windDir.y);
    const float3 crossDir3 = float3(-windDir.y, 0.0f, windDir.x);

    //----------------------------------------------------------
    // 突風の波。草と同じ式（風向きへ流れる正弦波）を根元の位置で読むので、
    // 草がなびく帯が通るのと同じ瞬間に木も大きく倒れる
    //----------------------------------------------------------
    const float wavelength = max(gustParams.x, 1.0f);
    const float gust = sin(dot(treeBase.xz, windDir) / wavelength - time * (gustParams.y / wavelength)) * 0.5f + 0.5f;

    //----------------------------------------------------------
    // 木全体のしなり。根元は動かさず、高さの2乗に比例して風下へ倒す
    // （幹がしなる形になる。1乗だと木が根元から棒のように傾いて見える）。
    // いつも少し風下へ寄せたうえで、木ごとの位相でゆっくり揺り戻し、突風で大きく倒す
    //----------------------------------------------------------
    const float heightRatio = max(worldPos.y - treeBase.y, 0.0f) / max(gustParams.w, 1.0f);
    const float bendWeight = heightRatio * heightRatio;
    const float phase = TreePhase(treeBase.xz);

    const float lean = 0.35f + 0.65f * sin(time * swayParams.y + phase) + gust * gustParams.z * 2.0f;
    float3 offset = windDir3 * (swayParams.x * bendWeight * lean);

    // 風と直交する向きにも小さく揺らして、往復運動が一直線に見えないようにする
    offset += crossDir3 * (swayParams.x * bendWeight * 0.3f * sin(time * swayParams.y * 0.77f + phase * 1.3f));

    //----------------------------------------------------------
    // 攻撃で揺れている木。根元の位置で自分のスロットを探し、CPUのばねが出したずれを
    // しなりと同じ重み（高さの2乗）で足す。大きく揺れている間は葉のそよぎも強める
    //----------------------------------------------------------
    float flutterBoost = 0.0f;
    const uint slotCount = min((uint)shakeParams.x, kMaxShakingTrees);
    [loop]
    for(uint s = 0; s < slotCount; ++s)
    {
        const float2 toSlot = treeBase.xz - shakeSlots[s].xy;
        if(dot(toSlot, toSlot) < 1.0f)
        {
            offset += float3(shakeSlots[s].z, 0.0f, shakeSlots[s].w) * bendWeight;
            flutterBoost = length(shakeSlots[s].zw) * shakeParams.y;
            break;
        }
    }

    //----------------------------------------------------------
    // 葉のそよぎ。位相を頂点の位置からなめらかに作るので、隣り合う葉は似た動き、
    // 離れた葉は違う動きになり、茂みの表面を波が走るように見える。
    // 葉カード1枚の4隅の位相差は小さいので、板が大きくゆがむことはない
    //----------------------------------------------------------
    if(swayParams.z > 0.0f)
    {
        const float leafPhase = dot(worldPos, float3(0.021f, 0.017f, 0.025f)) * kTreeTwoPi + phase;
        const float flutter = sin(time * swayParams.w + leafPhase);
        const float lift = sin(time * swayParams.w * 1.7f + leafPhase * 1.3f);
        offset += (windDir3 * flutter + float3(0.0f, 0.5f * lift, 0.0f)) * (swayParams.z * (0.6f + gust + flutterBoost));
    }

    return offset;
}

#endif    // COMBAT_TREE_SWAY_HLSLI
