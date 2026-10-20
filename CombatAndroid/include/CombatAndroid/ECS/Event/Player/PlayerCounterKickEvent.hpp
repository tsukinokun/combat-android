//-------------------------------------------------------------
//! @file   PlayerCounterKickEvent.hpp
//! @brief  PlayerCounterKickEvent構造体の宣言
//-------------------------------------------------------------
#pragma once

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerCounterKickEvent
    //! @brief  ジャスト回避の後の反撃キックを始めた通知。SlowMotionControllerが世界の時間を遅くする
    //! @note   プレイヤー自身はキックの間だけスローを打ち消して実時間で動く（PlayerAnimationSystem）ので、
    //!         周りが止まった中を高速で詰めて蹴る見た目になる
    //-------------------------------------------------------------
    struct PlayerCounterKickEvent {
        entt::entity player;          //!< キックしたプレイヤー
        float        slowScale;       //!< 遅くなりきったときの時間の倍率
        float        holdSeconds;     //!< その倍率を保つ秒（実時間）
        float        easeOutSeconds;  //!< 倍率1へ戻す秒（実時間）
    };
}    // namespace CombatAndroid::ECS
