//-------------------------------------------------------------
//! @file   PlayerHeavyStrikeEvent.hpp
//! @brief  プレイヤーが連撃の最終段に入った際のイベント
//-------------------------------------------------------------
#pragma once

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct PlayerHeavyStrikeEvent
    //! @brief  連撃の最終段（3段目）の打撃感を出す通知イベント。
    //!         PlayerAnimationSystemが最終段に入った瞬間にPublishし、TpsCameraSystemが
    //!         インパクトの瞬間に画面を揺らす。PlayerFinisherEventと違い、範囲攻撃の有無を問わず全武器で出る
    //-------------------------------------------------------------
    struct PlayerHeavyStrikeEvent {
        entt::entity player;         //!< 攻撃したプレイヤーエンティティ
        float        impactDelay;    //!< 最終段に入ってからインパクト（叩きつけ・当たり判定の開始）までの秒数
    };

}    // namespace CombatAndroid::ECS
