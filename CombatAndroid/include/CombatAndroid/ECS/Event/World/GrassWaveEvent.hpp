//-------------------------------------------------------------
//! @file   GrassWaveEvent.hpp
//! @brief  草を衝撃波で揺らす通知イベント
//-------------------------------------------------------------
#pragma once

#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct GrassWaveEvent
    //! @brief  中心から輪が広がり、範囲の草を外へ倒して揺らす。
    //!         CombatSystemが連撃3段目のインパクト（範囲攻撃の発動と同じ瞬間）にPublishし、
    //!         GrassFieldSystemが受け取ってGrass.vs.hlslへ渡す
    //-------------------------------------------------------------
    struct GrassWaveEvent {
        hlslpp::float3 center;    //!< 波の中心（ワールド座標。高さは使わない）
        float          radius;    //!< 波が届く半径
    };

}    // namespace CombatAndroid::ECS
