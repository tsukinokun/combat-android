//-------------------------------------------------------------
//! @file   PlayerFinisherEvent.hpp
//! @brief  プレイヤーが大技を出した際のイベント
//-------------------------------------------------------------
#pragma once

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct PlayerFinisherEvent
    //! @brief  大技の演出に使う通知イベント。受け取った側はどちらもインパクトの瞬間に演出を始める
    //!         （カメラのズーム：TpsCameraSystem、世界のスロー：SlowMotionController）。
    //!         PlayerAnimationSystemが大技に入った瞬間にPublishする。大技とは次のもの：
    //!         - 範囲攻撃を持つ段（ウォーハンマー・グレートソードの連撃3段目）
    //!         - 最大段階（紫）まで溜めてからの解放（バトルアックス）
    //-------------------------------------------------------------
    struct PlayerFinisherEvent {
        entt::entity player;         //!< 大技を出したプレイヤーエンティティ
        float        impactDelay;    //!< 大技に入ってからインパクト（範囲攻撃の発動・斬撃弾の射出）までの秒数
    };

}    // namespace CombatAndroid::ECS
