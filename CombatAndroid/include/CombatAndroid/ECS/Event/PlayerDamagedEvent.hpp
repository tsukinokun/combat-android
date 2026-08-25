//-------------------------------------------------------------
//! @file   PlayerDamagedEvent.hpp
//! @brief  プレイヤーが敵の攻撃を受けた際のイベント
//-------------------------------------------------------------
#pragma once

#include <entt/entt.hpp>
#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct PlayerDamagedEvent
    //! @brief  被弾演出（点滅・画面フラッシュ等。PlayerDamageEffectSystem）の副作用処理に使う通知イベント。
    //!         CombatSystemが敵の攻撃当たり判定でプレイヤーへダメージを与えた瞬間にPublishする
    //!         （WeaponHitEventと同じ流儀。無敵時間中はダメージが発生しないためPublishされない）
    //-------------------------------------------------------------
    struct PlayerDamagedEvent {
        entt::entity   attacker;      //!< 攻撃した敵エンティティ
        entt::entity   player;        //!< ダメージを受けたプレイヤーエンティティ
        float          damage;        //!< 実際に与えたダメージ量
        hlslpp::float3 hitPosition;   //!< ヒット位置（ワールド空間。演出用）
    };

}    // namespace CombatAndroid::ECS
