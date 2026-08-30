//-------------------------------------------------------------
//! @file    EnemySpawner.hpp
//! @brief   敵エンティティの生成処理の宣言
//! @author  山﨑愛
//! @note    以前は CombatAndroidScene::OnInitialize 内のローカルラムダだったため
//!          シーン構築時にしか呼べなかった。負荷試験（EnemyStressTestSystem）から
//!          実行時に湧かせられるよう、ここへ切り出している
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

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
        float               expReward = 10.0f;           //!< 撃破時にプレイヤーへ与えるEXP量

        //! BTがプレイヤーを追跡し続ける距離。これを超えると ZombieBehavior の
        //! MoveToPlayer / CanChase が Failure を返し、その場で待機したまま近づいてこなくなる。
        //! 既定値は EnemyComponent のものと揃えてあるため、明示しない限り従来の挙動は変わらない。
        //! フォグの外から湧かせる場合は、湧き半径より十分大きい値を湧かせる側が入れること
        float detectRange = 600.0f;

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
        std::string    boneName = "mixamorig:RightHand";
        hlslpp::float3 hitboxLocalOffset{0.0f, 0.0f, 0.0f};
        // endBoneNameが空なら従来通りboneName位置を中心とした球で判定する。
        // 設定すると、boneName→endBoneNameを芯線とするカプセルで判定する
        // （腕の振り抜きのように1点の球では部位を表現しきれない敵向け。EnemyAttackHitboxComponent参照）
        std::string    endBoneName = "";
        hlslpp::float3 endBoneLocalOffset{0.0f, 0.0f, 0.0f};
        float          hitboxRadius = 45.0f;
        float          hitboxDamage = 15.0f;
        float          hitStartTime = 0.40f;    //!< Attackへ入ってからの経過秒。ここから判定が有効になる
        float          hitDuration  = 0.20f;

        //-------------------------------------------------------------
        // 手に持たせる武器（Paladin等）。SpawnBehaviorEnemyが武器エンティティを別途生成し、
        // WeaponComponent::ownerをこの敵にして右手ボーンへ追従させる。
        // 生成した武器はEnemyHeldWeaponComponentへ記録され、撃破時にドロップされる。
        // 武器の性能はWeaponSpawner::ConfigureWeaponが決めるため、
        // 拾ったプレイヤーは手置きの武器とまったく同じものを手に入れる
        //-------------------------------------------------------------
        bool     hasHeldWeapon = false;                    //!< 手に武器を持たせるか
        WeaponId heldWeaponId  = WeaponId::Warhammer;    //!< 持たせる武器の種類
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

    //-------------------------------------------------------------
    //! @brief  Paladin 1体分の生成パラメータを作る関数（武器を明示指定する版）
    //! @param  context       [in] エンジンコンテキスト
    //! @param  spawnPosition [in] 出現位置
    //! @param  weaponId      [in] 持たせる武器の種類
    //! @return 生成パラメータ
    //! @note   Paladinは持っている武器によって攻撃モーション・間合い・威力が変わる。
    //!         抽選を伴わない決定的な版なので、シーンへの手置きや見た目の確認に使う
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemySpawnConfig MakePaladinConfig(Tsukino::EngineIntegration::EngineContext& context,
                                       const hlslpp::float3& spawnPosition,
                                       WeaponId weaponId);

    //-------------------------------------------------------------
    //! @brief  Paladin 1体分の生成パラメータを作る関数（武器をランダムに選ぶ版）
    //! @param  context       [in] エンジンコンテキスト
    //! @param  spawnPosition [in] 出現位置
    //! @return 生成パラメータ
    //! @note   EnemySpawnTableのEnemyConfigFactoryへ渡すのはこちら。
    //!         EnemyConfigFactoryは乱数生成器を引数に取らないため、抽選は.cpp側の
    //!         ファイルローカルなmt19937で行い、あとは武器を明示する上のオーバーロードへ委譲する
    //!         （シグネチャを変えると負荷試験・シーンの手置き側にも乱数生成器が要るようになり、
    //!         　それらは抽選を必要としないため割に合わない）
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemySpawnConfig MakePaladinConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition);
}    // namespace CombatAndroid::ECS
