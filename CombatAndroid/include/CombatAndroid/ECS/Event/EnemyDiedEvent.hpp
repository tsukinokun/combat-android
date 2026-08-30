//-------------------------------------------------------------
//! @file   EnemyDiedEvent.hpp
//! @brief  敵が死亡演出を終えて消滅する際のイベント
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <entt/entt.hpp>
#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @struct EnemyDiedEvent
    //! @brief  EXP玉のドロップ等、撃破報酬まわりの副作用処理に使う通知イベント。
    //!         各敵種のビヘイビアツリー（死亡アクション）から、消滅（QueueDestroy）
    //!         直前にPublishする
    //-------------------------------------------------------------
    struct EnemyDiedEvent {
        hlslpp::float3 position;      //!< 死亡位置（ワールド空間。EXP玉のスポーン地点に使う）
        int            expReward;     //!< 獲得EXP量

        //-------------------------------------------------------------
        // 撃破時に地面へ落とす武器（Paladin等が手に持っていたもの）。持っていなければentt::null。
        // 敵本体はPublishの直後にQueueDestroyされ、EnemyHeldWeaponComponentを
        // 引き直せる保証が無いため、武器エンティティはイベントに載せて運ぶ
        //-------------------------------------------------------------
        Tsukino::ECS::Entity heldWeaponEntity = entt::null;
    };

}    // namespace CombatAndroid::ECS
