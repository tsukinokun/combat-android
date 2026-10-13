//--------------------------------------------------------------
//! @file   FallingLeaf.vs.hlsl
//! @brief  攻撃で木から舞い落ちる葉の頂点シェーダー（インスタンス描画）
//! @note   1枚の四角形を、葉の枚数ぶんインスタンス描画する。葉ごとの位置・大きさ・回転・絵のマスは
//!         TreeFieldSystemが毎フレームCPUで動かして t16 の StructuredBuffer へ書く。
//!         出力はエンジンの ModelStatic.vs.hlsl と同じで、ピクセルシェーダーはエンジンの GBuffer.ps.hlsl
//!         （葉の形のくり抜きはマテリアルの alphaCutoff）
//--------------------------------------------------------------
#pragma pack_matrix(row_major)
#include "Scene.hlsli"

//--------------------------------------------------------------
//! @brief 葉1枚ぶんのデータ（TreeFieldSystem.cpp の FallingLeafInstance と一致させること。32バイト）
//--------------------------------------------------------------
struct LeafInstance
{
    float4 positionSize;    // xyz: ワールド座標, w: 一辺の長さ
    float4 rotation;        // x: ヨー, y: ピッチ, z: ロール（ラジアン）, w: 葉の絵のマス（0〜3）
};

StructuredBuffer<LeafInstance> leaves : register(t16);

struct VSInput
{
    float3 position : POSITION;    // -0.5〜0.5 の四角形（XY平面、法線は+Z）
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    uint   instanceID : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 curClip : TEXCOORD1;
    float4 prevClip : TEXCOORD2;
};

//--------------------------------------------------------------
//! ヨー・ピッチ・ロールから回転行列を作ります（ロール→ピッチ→ヨーの順に掛ける）。
//--------------------------------------------------------------
float3x3 MakeRotation(float yaw, float pitch, float roll)
{
    float sy, cy, sp, cp, sr, cr;
    sincos(yaw, sy, cy);
    sincos(pitch, sp, cp);
    sincos(roll, sr, cr);

    const float3x3 rz = float3x3(cr, sr, 0.0f, -sr, cr, 0.0f, 0.0f, 0.0f, 1.0f);
    const float3x3 rx = float3x3(1.0f, 0.0f, 0.0f, 0.0f, cp, sp, 0.0f, -sp, cp);
    const float3x3 ry = float3x3(cy, 0.0f, -sy, 0.0f, 1.0f, 0.0f, sy, 0.0f, cy);
    return mul(rz, mul(rx, ry));
}

VSOutput VSMain(VSInput input)
{
    const LeafInstance leaf = leaves[input.instanceID];
    const float3x3     rot  = MakeRotation(leaf.rotation.x, leaf.rotation.y, leaf.rotation.z);

    const float3 worldPos = mul(input.position * leaf.positionSize.w, rot) + leaf.positionSize.xyz;

    // 葉の絵のマス（2×2のアトラス）
    const uint   tile = (uint)leaf.rotation.w;
    const float2 tileOffset = float2(tile & 1u, (tile >> 1u) & 1u) * 0.5f;

    //----------------------------------------------------------
    // 法線。薄い葉は裏から見ても同じ明るさに見えるので、上を向いた側に揃え、
    // さらに上へ寄せて、落ちている間に裏返っても真っ暗にならないようにする
    //----------------------------------------------------------
    float3 normal = mul(float3(0.0f, 0.0f, 1.0f), rot);
    if(normal.y < 0.0f)
        normal = -normal;
    normal = normalize(normal + float3(0.0f, 0.8f, 0.0f));

    VSOutput output;
    output.worldPos = worldPos;
    output.curClip  = mul(float4(worldPos, 1.0f), viewProj);
    output.position = output.curClip;
    output.normal   = normal;
    output.uv       = tileOffset + input.uv * 0.5f;
    output.prevClip = output.curClip;    // 速度は出さない（小さく速く動く葉にモーションブラーは要らない）
    return output;
}
