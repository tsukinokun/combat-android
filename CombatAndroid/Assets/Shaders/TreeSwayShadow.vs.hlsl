//--------------------------------------------------------------
//! @file   TreeSwayShadow.vs.hlsl
//! @brief  風で揺れる木の影用頂点シェーダー
//! @note   エンジンの ShadowMapStatic.vs.hlsl と同じ出力に、本体（TreeSway.vs.hlsl）と
//!         同じ風のずれを足したもの。影も本体と一緒に揺れる。
//!         ピクセルシェーダーはエンジンの ShadowMap.ps.hlsl（葉のくり抜きと網点フェード）を使う
//--------------------------------------------------------------
#pragma pack_matrix(row_major)
#include "Scene.hlsli"
#include "TreeSway.hlsli"

cbuffer CBufferTransform : register(b1)
{
    matrix world;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    float4 worldPos = mul(float4(input.position, 1.0f), world);
    worldPos.xyz += TreeSwayOffset(worldPos.xyz, world[3].xyz, timeParams.x);

    VSOutput output;
    // シャドウパスが描画中のカスケード番号は shadowParams.z に入っている
    output.position = mul(worldPos, cascadeViewProj[(uint)shadowParams.z]);
    output.uv       = input.uv;
    return output;
}
