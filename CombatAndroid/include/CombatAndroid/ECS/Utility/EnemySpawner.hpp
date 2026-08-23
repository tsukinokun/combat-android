//-------------------------------------------------------------
//! @file    EnemySpawner.hpp
//! @brief   敵エンティティの生成処理の宣言
//! @author  山﨑愛
//! @note    以前は CombatAndroidScene::OnInitialize 内のローカルラムダだったため
//!          シーン構築時にしか呼べなかった。負荷試験（EnemyStressTestSystem）から
//!          実行時に湧かせられるよう、ここへ切り出している
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Registry/Registry.hpp>
#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Engine/Asset/AssetHandle.hpp>

#include <hlsl++.h>

#include <string>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EnemySpawnConfig
    //! @brief  SpawnBehaviorEnemyへ渡す1体分の生成パラメータ
    //-------------------------------------------------------------
    struct EnemySpawnConfig {
        hlslpp::float3      spawnPosition;
        float               moveSpeed;
        float               maxHealth;
        Tsukino::Core::Path modelPath;
        hlslpp::float3      scale;
        float               bodyRadius;                  //!< 武器ヒット判定用カプセルの半径
        float               bodyHalfHeight;              //!< 武器ヒット判定用カプセルの半高さ
        float               attackRange;                 //!< BTが攻撃へ移る距離
        float               knockbackDamageThreshold;    //!< この値以上の単発ダメージでノックバックする

        //! アニメーションの再生開始位置（秒）。
        //! 負荷試験で大量に湧かせるとき、全個体が同じ位置から再生されると
        //! AnimationSystemが毎フレーム完全に同じ分岐・同じキーフレームを辿ることになり、
        //! キャッシュに乗りすぎて実態より軽く測れてしまう。個体ごとにずらすために使う
        float initialAnimationTime = 0.0f;

        Tsukino::Asset::AssetHandle walkClip;
        Tsukino::Asset::AssetHandle attackClip;
        Tsukino::Asset::AssetHandle knockbackClip;
        Tsukino::Asset::AssetHandle deathClip;

        // 敵の攻撃当たり判定（EnemyAttackHitboxComponent）
        std::string    handBoneName = "mixamorig:RightHand";
        hlslpp::float3 hitboxLocalOffset{0.0f, 0.0f, 0.0f};
        float          hitboxRadius = 45.0f;
        float          hitboxDamage = 15.0f;
        float          hitStartTime = 0.40f;    //!< Attackへ入ってからの経過秒。ここから判定が有効になる
        float          hitDuration  = 0.20f;
    };

    //-------------------------------------------------------------
    //! @brief  敵を1体生成する関数
    //! @param  registry [in] エンティティレジストリ
    //! @param  context  [in] エンジンコンテキスト（AssetManagerの取得に使う）
    //! @param  config   [in] 生成パラメータ
    //! @return 生成した敵本体のエンティティ
    //! @note   1体につき「本体・HPバー背景・HPバー残量」の3エンティティを生成する。
    //!         破棄する際は HealthComponent が持つ hpBarBackgroundEntity /
    //!         hpBarFillEntity も併せて QueueDestroy すること
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnBehaviorEnemy(Tsukino::ECS::Registry& registry,
                                            Tsukino::EngineIntegration::EngineContext& context,
                                            const EnemySpawnConfig& config);

    //-------------------------------------------------------------
    //! @brief  SmallZombie 1体分の生成パラメータを作る関数
    //! @param  context       [in] エンジンコンテキスト（アニメーションクリップのロードに使う）
    //! @param  spawnPosition [in] 出現位置
    //! @return 生成パラメータ
    //! @note   AssetManager::Load はパスでキャッシュされるため、2体目以降は
    //!         ハンドルを引き直すだけで再ロードは発生しない
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemySpawnConfig MakeSmallZombieConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition);

    //-------------------------------------------------------------
    //! @brief  BigZombie 1体分の生成パラメータを作る関数
    //! @param  context       [in] エンジンコンテキスト
    //! @param  spawnPosition [in] 出現位置
    //! @return 生成パラメータ
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemySpawnConfig MakeBigZombieConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition);
}    // namespace CombatAndroid::ECS
