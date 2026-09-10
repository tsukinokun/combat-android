//--------------------------------------------------------------
//! @file   Ground.vs.hlsl
//! @brief  地面（土の板）用頂点シェーダー
//! @author 山﨑愛
//--------------------------------------------------------------
// 地面はプレイヤーへ追従して動く板（GroundFollowSystem）。UVをメッシュに
// 焼き込んだままだと、板の位置が動いてもUVはローカル座標のままなので、
// テクスチャが板にくっついて一緒に移動するだけになり「歩いても地面の
// 模様が流れない」という不自然な見た目になる。
//
// そこでUVをワールドXZ座標から算出し、テクスチャをワールド空間に固定する。
// 板がどれだけ追従して動いても、同じワールド座標は常に同じUVへ
// マッピングされるため、地面越しに見ると模様が正しく流れて見える
// （草の「位置はワールド座標のハッシュで決める」のと同じ考え方）。
//
// 出力は ModelStatic.vs.hlsl と同じ VSOutput なので、ピクセルシェーダーは
// 既存の GBuffer.ps.hlsl をそのまま使える。
#pragma pack_matrix(row_major)

//--------------------------------------------------------------
// 定数バッファ：シーン (b0)
// ModelStatic.vs.hlslのCBufferSceneと同じ並び
//--------------------------------------------------------------
cbuffer CBufferScene : register(b0)
{
    matrix view;
    matrix projection;
    matrix viewProj;
    matrix invViewProj;
    matrix lightViewProj;
    float4 lightDir;
    float4 lightColor;
    float4 cameraPos;
    matrix prevViewProj;
};

//--------------------------------------------------------------
// 定数バッファ：トランスフォーム (b1)
// ModelStatic.vs.hlslのCBufferTransformと同じ並び。DrawCommand::transformから
// エンジンが自動で埋める（ゲーム側で用意する必要はない）
//--------------------------------------------------------------
cbuffer CBufferTransform : register(b1)
{
    matrix world;
    matrix prevWorld;
    float4 motionFlags;    // x: 1=前フレーム有効 / 0=速度ゼロ
};

//--------------------------------------------------------------
// 定数バッファ：地面 (b12 = CBSlot::User0 のゲーム予約枠)
// CombatAndroid/ECS/System/GroundVisualSystem.hpp の CBufferGround と
// 1バイト単位で一致させること
//--------------------------------------------------------------
cbuffer CBufferGround : register(b12)
{
    float4 tileParams;    // x: タイル1枚ぶんのワールド距離, yzw: 予約
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;    // 未使用（UVはワールド座標から算出する）
};

//--------------------------------------------------------------
// 出力
// ModelStatic.vs.hlsl の VSOutput と完全に同じにすること
//--------------------------------------------------------------
struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float4 curClip  : TEXCOORD1;
    float4 prevClip : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 localPos = float4(input.position, 1.0f);
    float4 worldPos = mul(localPos, world);

    output.worldPos = worldPos.xyz;
    output.curClip   = mul(worldPos, viewProj);
    output.position  = output.curClip;
    output.normal    = normalize(mul(input.normal, (float3x3)world));

    // ワールドXZをタイルサイズで割った値をそのままUVにする
    const float tileSize = max(tileParams.x, 1.0f);
    output.uv = worldPos.xz / tileSize;

    // 前フレームのクリップ座標（速度バッファ用）。ModelStatic.vs.hlslと同じ
    if(motionFlags.x > 0.5f)
    {
        float4 prevWorldPos = mul(localPos, prevWorld);
        output.prevClip = mul(prevWorldPos, prevViewProj);
    }
    else
    {
        output.prevClip = output.curClip;
    }

    return output;
}
