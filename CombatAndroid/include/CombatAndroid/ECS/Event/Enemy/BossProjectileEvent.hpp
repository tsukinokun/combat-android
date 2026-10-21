//-------------------------------------------------------------
//! @file   BossProjectileEvent.hpp
//! @brief  ボスが弾を撃つときのイベント
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <hlsl++.h>

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct BossProjectileEvent
    //! @brief  ボスの技（糸弾など）が弾を1発撃つ通知
    //! @note   ビヘイビアツリーはViewの反復中に動くので、そこではエンティティを作らない。
    //!         この通知を出し、実際の生成はBossDirectorSystem::Updateで行う（BossSummonEventと同じ作法）
    //-------------------------------------------------------------
    struct BossProjectileEvent {
        Tsukino::ECS::Entity owner = entt::null;                     //!< 撃ったボス（被弾演出のヒットストップに使う）
        hlslpp::float3       origin{0.0f, 0.0f, 0.0f};               //!< 撃ち出す位置
        hlslpp::float3       direction{0.0f, 0.0f, 1.0f};            //!< 進む向き（正規化済み。撃ち下ろしの弾は下向きの成分を持つ）
        float                speed       = 900.0f;                   //!< 速さ（ユニット/秒）
        float                damage      = 10.0f;                    //!< 当たったときのダメージ（倍率適用後）
        float                radius      = 40.0f;                    //!< 当たりの半径
        float                range       = 1600.0f;                  //!< 届く距離
        float                slowSeconds = 0.0f;                     //!< 当たったときの鈍足の長さ（秒）
        float                slowScale   = 1.0f;                     //!< 鈍足中の移動速度の倍率
        std::string          prefab;                                 //!< 弾のPrefab（Assets/Prefabs/ からの相対名）。空なら Enemy/WebShot
    };
}    // namespace CombatAndroid::ECS
