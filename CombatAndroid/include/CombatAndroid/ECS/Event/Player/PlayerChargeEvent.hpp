//-------------------------------------------------------------
//! @file   PlayerChargeEvent.hpp
//! @brief  プレイヤーが溜め攻撃を解放した際のイベント
//-------------------------------------------------------------
#pragma once

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct PlayerChargeReleasedEvent
    //! @brief  溜め攻撃の効果音（ChargeSoundSystem）に使う通知イベント。
    //!         PlayerAnimationSystemが溜めを解放した瞬間にPublishする。
    //!         受け取った側は fireDelay 秒後（斬撃弾が飛び出す瞬間）に「ドン」を鳴らす
    //-------------------------------------------------------------
    struct PlayerChargeReleasedEvent {
        entt::entity player;       //!< 解放したプレイヤーエンティティ
        float        fireDelay;    //!< 解放から斬撃弾が飛び出すまでの秒数
    };

}    // namespace CombatAndroid::ECS
