//--------------------------------------------------------------
//! @file   TreeSway.vs.hlsl
//! @brief  風で揺れる木の頂点シェーダー（GBufferパス）
//! @note   入出力はエンジンの ModelStatic.vs.hlsl と同じにしてあり、ピクセルシェーダーは
//!         エンジンの GBuffer.ps.hlsl をそのまま使う（ライティング・霧・くり抜き・網点フェードが効く）。
//!         違いはワールド座標に風のずれ（TreeSway.hlsli）を足すことだけ。
//!         影は同じずれを足す TreeSwayShadow.vs.hlsl で描く（DrawCommand::shadowPipeline）
//--------------------------------------------------------------
#pragma pack_matrix(row_major)
#include "Scene.hlsli"
#include "TreeSway.hlsli"

//--------------------------------------------------------------
//! @brief トランスフォーム定数バッファ (b1)
//--------------------------------------------------------------
cbuffer CBufferTransform : register(b1)
{
    matrix world;
    matrix prevWorld;      // 前フレームのワールド行列
    float4 motionFlags;    // x: 1=前フレーム有効 / 0=速度ゼロ, y: 網点フェード
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

//--------------------------------------------------------------
//! @brief VS出力構造体（ModelStatic.vs.hlsl と同じ）
//--------------------------------------------------------------
struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 curClip : TEXCOORD1;
    float4 prevClip : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    const float4 localPos = float4(input.position, 1.0f);
    const float  time     = timeParams.x;

    // ワールド座標へ変換し、風のずれを足す。根元はワールド行列の平行移動
    float4 worldPos = mul(localPos, world);
    worldPos.xyz += TreeSwayOffset(worldPos.xyz, world[3].xyz, time);

    output.worldPos = worldPos.xyz;
    output.curClip  = mul(worldPos, viewProj);
    output.position = output.curClip;
    output.normal   = normalize(mul(input.normal, (float3x3)world));    // 揺れは小さいので法線はそのまま使う

    //----------------------------------------------------------
    // 葉だけ：法線がカメラから背を向けすぎないよう、カメラの方へ寄せる。
    // 葉の板の法線は茂みの中心から外向きの作り物（ふんわり明るく見せるため）で、板は両面描画。
    // 茂みの縁では法線がカメラからほぼ背を向け（N・V≒0）、かすめる角度の反射（フレネル）が
    // ほぼ100%になって太陽や空が映り込み、白く飛んでいた。N・Vを下限まで持ち上げると、
    // 外向きの陰影はほぼ残したまま映り込みが消え、逆光の太陽もN・L≦0になって乗らなくなる
    //----------------------------------------------------------
    const float minFacing = shakeParams.z;
    if(minFacing > 0.0f)
    {
        const float3 toCamera = normalize(cameraPos.xyz - worldPos.xyz);
        const float  facing   = dot(output.normal, toCamera);
        if(facing < minFacing)
            output.normal = normalize(output.normal + toCamera * (minFacing - facing));
    }
    output.uv       = input.uv;

    //----------------------------------------------------------
    // 前フレームのクリップ座標（速度バッファ用）。前フレームの時刻のずれで作るので、
    // 揺れによる動きもモーションブラーの速度に正しく入る
    //----------------------------------------------------------
    if(motionFlags.x > 0.5f)
    {
        float4 prevWorldPos = mul(localPos, prevWorld);
        prevWorldPos.xyz += TreeSwayOffset(prevWorldPos.xyz, prevWorld[3].xyz, time - timeParams.y);
        output.prevClip = mul(prevWorldPos, prevViewProj);
    }
    else
    {
        output.prevClip = output.curClip;
    }

    return output;
}
