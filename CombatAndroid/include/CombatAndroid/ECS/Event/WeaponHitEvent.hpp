//-------------------------------------------------------------
//! @file   WeaponHitEvent.hpp
//! @brief  武器が敵にヒットした際のイベント
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once

#include <entt/entt.hpp>
#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct WeaponHitEvent
    //! @brief  ヒットストップ・エフェクト・SE等の副作用処理に使う通知イベント
    //-------------------------------------------------------------
    struct WeaponHitEvent {
        entt::entity   attacker;      //!< 攻撃した側のエンティティ（武器の所有者）
        entt::entity   weapon;        //!< ヒットした武器エンティティ
        entt::entity   target;        //!< ヒットを受けた敵エンティティ
        hlslpp::float3 hitPosition;      //!< ヒット位置（ワールド空間。敵の足元原点。DamageNumberSystemが高さ補正して使う）
        hlslpp::float3 contactPosition;  //!< 判定に使った武器/斬撃弾の中心位置（ワールド空間。演出用）。
                                          //!< hitPositionと違い、攻撃ごとに実際の当たり所へ近い高さ・位置になる
        float          damage;        //!< 実際に与えたダメージ量
        bool           killed;        //!< このヒットで倒したか（撃破音・演出の起点に使う）
    };

}    // namespace CombatAndroid::ECS
