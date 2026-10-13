//-------------------------------------------------------------
//! @file   TreeHitEvent.hpp
//! @brief  攻撃が木に当たったことを知らせるイベント
//-------------------------------------------------------------
#pragma once

#include <hlsl++.h>

#include <cstdint>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct TreeHitEvent
    //! @brief  木を大きく揺らし、葉を落とすための通知。
    //!         CombatSystem（武器の振り）とProjectileSystem（斬撃弾）が、当たり判定で木の幹
    //!         （TreeColliderComponent）を拾ったときにPublishし、TreeFieldSystemが受け取る
    //-------------------------------------------------------------
    struct TreeHitEvent {
        std::uint64_t  cellKey   = 0;                                  //!< 当たった木の升目のキー
        hlslpp::float3 direction = hlslpp::float3(1.0f, 0.0f, 0.0f);   //!< 木を倒す向き（攻撃が動いていた向き。水平成分を使う）
        float          strength  = 1.0f;                               //!< 強さの倍率（揺れと落ちる葉の数に掛ける）
    };

}    // namespace CombatAndroid::ECS
