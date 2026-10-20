//-------------------------------------------------------------
//! @file   PlayerJustDodgeEvent.hpp
//! @brief  PlayerJustDodgeEvent構造体の宣言
//-------------------------------------------------------------
#pragma once

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerJustDodgeEvent
    //! @brief  ジャスト回避が決まった通知。TryJustDodgeが毎回出し、TpsCameraSystemがカメラを寄せる
    //! @note   スロー（PlayerFinisherEvent）は雑魚では間を空けて出すが、こちらは毎回出る
    //-------------------------------------------------------------
    struct PlayerJustDodgeEvent {
        entt::entity player;      //!< 避けたプレイヤー
        entt::entity attacker;    //!< 避けた攻撃の相手
    };
}    // namespace CombatAndroid::ECS
