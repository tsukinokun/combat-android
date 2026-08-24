//-------------------------------------------------------------
//! @file   EnemyDiedEvent.hpp
//! @brief  敵が死亡演出を終えて消滅する際のイベント
//-------------------------------------------------------------
#pragma once

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
    };

}    // namespace CombatAndroid::ECS
