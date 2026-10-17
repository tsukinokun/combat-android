//-------------------------------------------------------------
//! @file   BossSummonEvent.hpp
//! @brief  ボスが雑魚を呼ぶときのイベント
//-------------------------------------------------------------
#pragma once

#include <hlsl++.h>

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct BossSummonEvent
    //! @brief  ボスの技（咆哮など）が雑魚を呼ぶ通知
    //! @note   ビヘイビアツリーはViewの反復中に動くので、そこではエンティティを作らない。
    //!         この通知を出し、実際の生成はBossDirectorSystem::Updateで行う（EnemyDiedEventと同じ作法）
    //-------------------------------------------------------------
    struct BossSummonEvent {
        hlslpp::float3 center;    //!< 呼ぶ中心（ボスの位置）
        int            count;     //!< 呼ぶ数
        float          radius;    //!< 中心から置く距離
        std::string    prefab;              //!< 呼ぶ敵のPrefab（Assets/Prefabs/ からの相対名）。空ならSmallZombie
        float          sizeScale   = 1.0f;  //!< 呼ぶ敵の大きさの倍率
        float          healthScale = 1.0f;  //!< 呼ぶ敵のHPの倍率
    };
}    // namespace CombatAndroid::ECS
