//-------------------------------------------------------------
//! @file   CombatDebugDraw.hpp
//! @brief  当たり判定の可視化用ワイヤーフレーム描画（_DEBUGビルドのみ）
//! @author 山﨑愛
//! @note   武器スイング・敵の攻撃判定（CombatSystem）と斬撃弾（ProjectileSystem）が
//!         同じ見た目で判定を確認できるよう共有する
//-------------------------------------------------------------
#pragma once

#ifdef _DEBUG
#include <Tsukino/Renderer/Renderer.hpp>
#include <Tsukino/GraphicsCommon/Vertex/DebugVertex.hpp>

#include <hlsl++.h>

#include <cmath>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  当たり判定範囲を目視確認できるよう、XZ平面上の円をワイヤーフレームで描画する
    //-------------------------------------------------------------
    inline void DrawWireCircleXZ(Tsukino::Renderer::Renderer* renderer, const hlslpp::float3& center, float radius, const hlslpp::float4& color) {
        constexpr int segments = 24;
        Tsukino::GraphicsCommon::DebugVertex prev{
            {center.x + radius, center.y, center.z},
            {color.x, color.y, color.z, color.w}
        };
        for(int i = 1; i <= segments; ++i) {
            float angle = (2.0f * 3.14159265f) * (static_cast<float>(i) / static_cast<float>(segments));
            Tsukino::GraphicsCommon::DebugVertex next{
                {center.x + std::cos(angle) * radius, center.y, center.z + std::sin(angle) * radius},
                {color.x, color.y, color.z, color.w}
            };
            renderer->DrawDebugLine(prev, next);
            prev = next;
        }
    }

    //-------------------------------------------------------------
    //! @brief  任意軸のカプセル（halfHeightがほぼ0なら球）をワイヤーフレームで描画する。
    //!         rotationはJolt/OverlapCapsuleと同じくローカルY軸をカプセルの軸として扱う
    //-------------------------------------------------------------
    inline void DrawWireCapsule(Tsukino::Renderer::Renderer* renderer, const hlslpp::float3& center,
                         const hlslpp::quaternion& rotation, float radius, float halfHeight, const hlslpp::float4& color) {
        constexpr float kPi = 3.14159265f;

        hlslpp::float3 axisX = hlslpp::mul(hlslpp::float3(1.0f, 0.0f, 0.0f), rotation);
        hlslpp::float3 axisY = hlslpp::mul(hlslpp::float3(0.0f, 1.0f, 0.0f), rotation);
        hlslpp::float3 axisZ = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), rotation);

        // 中心cから直交2軸u,v上に、startAngle〜endAngleの円弧をsegments分割で描く
        auto drawArc = [&](const hlslpp::float3& c, const hlslpp::float3& u, const hlslpp::float3& v,
                            float startAngle, float endAngle, int segments) {
            hlslpp::float3 firstPos = c + u * (std::cos(startAngle) * radius) + v * (std::sin(startAngle) * radius);
            Tsukino::GraphicsCommon::DebugVertex prev{
                {firstPos.x, firstPos.y, firstPos.z},
                {color.x, color.y, color.z, color.w}
            };
            for(int i = 1; i <= segments; ++i) {
                float          angle = startAngle + (endAngle - startAngle) * (static_cast<float>(i) / static_cast<float>(segments));
                hlslpp::float3 pos    = c + u * (std::cos(angle) * radius) + v * (std::sin(angle) * radius);
                Tsukino::GraphicsCommon::DebugVertex next{
                    {pos.x, pos.y, pos.z},
                    {color.x, color.y, color.z, color.w}
                };
                renderer->DrawDebugLine(prev, next);
                prev = next;
            }
        };

        constexpr int kFullSegments = 24;

        if(halfHeight <= 1e-4f) {
            // 半径のみ＝球として3方向の円で簡易表現する
            drawArc(center, axisX, axisZ, 0.0f, 2.0f * kPi, kFullSegments);
            drawArc(center, axisX, axisY, 0.0f, 2.0f * kPi, kFullSegments);
            drawArc(center, axisZ, axisY, 0.0f, 2.0f * kPi, kFullSegments);
            return;
        }

        hlslpp::float3 top    = center + axisY * halfHeight;
        hlslpp::float3 bottom = center - axisY * halfHeight;

        // 円柱側面の断面円（両端）
        drawArc(top, axisX, axisZ, 0.0f, 2.0f * kPi, kFullSegments);
        drawArc(bottom, axisX, axisZ, 0.0f, 2.0f * kPi, kFullSegments);

        // 半球キャップ（上下2端×2平面の円弧）
        constexpr int kCapSegments = kFullSegments / 2;
        drawArc(top, axisX, axisY, 0.0f, kPi, kCapSegments);
        drawArc(top, axisZ, axisY, 0.0f, kPi, kCapSegments);
        drawArc(bottom, axisX, axisY, 0.0f, -kPi, kCapSegments);
        drawArc(bottom, axisZ, axisY, 0.0f, -kPi, kCapSegments);

        // 円柱側面をつなぐ縦線（0°/90°/180°/270°）
        for(int i = 0; i < 4; ++i) {
            float          angle  = (kPi * 0.5f) * static_cast<float>(i);
            hlslpp::float3 offset = axisX * (std::cos(angle) * radius) + axisZ * (std::sin(angle) * radius);
            hlslpp::float3 a      = top + offset;
            hlslpp::float3 b      = bottom + offset;
            renderer->DrawDebugLine(
                Tsukino::GraphicsCommon::DebugVertex{{a.x, a.y, a.z}, {color.x, color.y, color.z, color.w}},
                Tsukino::GraphicsCommon::DebugVertex{{b.x, b.y, b.z}, {color.x, color.y, color.z, color.w}});
        }
    }
}    // namespace CombatAndroid::ECS
#endif    // _DEBUG
