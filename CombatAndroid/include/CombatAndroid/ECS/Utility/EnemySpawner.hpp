//-------------------------------------------------------------
//! @file    EnemySpawner.hpp
//! @brief   敵エンティティの生成処理の宣言
//! @author  山﨑愛
//! @note    以前は CombatAndroidScene::OnInitialize 内のローカルラムダだったため
//!          シーン構築時にしか呼べなかった。負荷試験（EnemyStressTestSystem）から
//!          実行時に湧かせられるよう、ここへ切り出している。
//!          敵の素の値は Assets/Prefabs/Enemy/<名前>/ のPrefabが持ち、ここは
//!          「どのPrefabを・どこに・どれだけ強化して」生成するかだけを扱う
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

#include <Tsukino/Core/ECS/Registry/Registry.hpp>

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
    //! @note   値そのものはPrefabにあり、ここが持つのは生成位置と、Prefabの値へ掛ける倍率だけ。
    //!         危険度（ApplyEnemyDifficulty）とエリート（ApplyEliteModifiers）は倍率を積み、
    //!         SpawnBehaviorEnemyが生成直後のComponentへまとめて掛ける
    //-------------------------------------------------------------
    struct EnemySpawnConfig {
        const char*    prefabName = "Enemy/SmallZombie";    //!< Assets/Prefabs/ からの相対名
        hlslpp::float3 spawnPosition{0.0f, 0.0f, 0.0f};

        //! BTがプレイヤーを追跡し続ける距離の上書き。0以下ならPrefabの値のまま。
        //! フォグの外から湧かせる場合は、湧き半径より十分大きい値を湧かせる側が入れること
        float detectRange = 0.0f;

        //! アニメーションの再生開始位置（秒）。
        //! 負荷試験で大量に湧かせるとき、全個体が同じ位置から再生されると
        //! AnimationSystemが毎フレーム完全に同じ分岐・同じキーフレームを辿ることになり、
        //! キャッシュに乗りすぎて実態より軽く測れてしまう。個体ごとにずらすために使う
        float initialAnimationTime = 0.0f;

        //-------------------------------------------------------------
        // Prefabの値へ掛ける倍率（危険度・エリート）
        //-------------------------------------------------------------
        float healthScale             = 1.0f;    //!< 最大HP
        float expScale                = 1.0f;    //!< 撃破時のEXP
        float attackScale             = 1.0f;    //!< 攻撃判定のダメージ
        float knockbackThresholdScale = 1.0f;    //!< ひるみ閾値
        float moveSpeedScale          = 1.0f;    //!< 移動速度
        float sizeScale               = 1.0f;    //!< 見た目・体の当たり・攻撃範囲

        //-------------------------------------------------------------
        // 手に持たせる武器（Paladin等）。SpawnBehaviorEnemyが武器エンティティを別途生成し、
        // WeaponComponent::ownerをこの敵にして右手ボーンへ追従させる。
        // 攻撃モーション・間合い・判定は武器ごとの値（PaladinWeaponAttacks.json）で上書きする。
        // 生成した武器はEnemyHeldWeaponComponentへ記録され、撃破時にドロップされる
        //-------------------------------------------------------------
        bool     hasHeldWeapon = false;                  //!< 手に武器を持たせるか
        WeaponId heldWeaponId  = WeaponId::Warhammer;    //!< 持たせる武器の種類
    };

    //-------------------------------------------------------------
    //! @brief  敵を1体生成する関数
    //! @param  registry [in] エンティティレジストリ
    //! @param  context  [in] エンジンコンテキスト（PrefabFactoryの取得に使う）
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
    //! @param  context       [in] エンジンコンテキスト（未使用。EnemyConfigFactoryと揃えるため）
    //! @param  spawnPosition [in] 出現位置
    //! @return 生成パラメータ
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemySpawnConfig MakeSmallZombieConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition);

    //-------------------------------------------------------------
    //! @brief  BigZombie 1体分の生成パラメータを作る関数
    //! @param  context       [in] エンジンコンテキスト（未使用。EnemyConfigFactoryと揃えるため）
    //! @param  spawnPosition [in] 出現位置
    //! @return 生成パラメータ
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemySpawnConfig MakeBigZombieConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition);

    //-------------------------------------------------------------
    //! @brief  Paladin 1体分の生成パラメータを作る関数（武器を明示指定する版）
    //! @param  context       [in] エンジンコンテキスト（未使用。EnemyConfigFactoryと揃えるため）
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
    //! @param  context       [in] エンジンコンテキスト（未使用。EnemyConfigFactoryと揃えるため）
    //! @param  spawnPosition [in] 出現位置
    //! @return 生成パラメータ
    //! @note   EnemySpawnTableのEnemyConfigFactoryへ渡すのはこちら。
    //!         EnemyConfigFactoryは乱数生成器を引数に取らないため、抽選は.cpp側の
    //!         ファイルローカルなmt19937で行い、あとは武器を明示する上のオーバーロードへ委譲する
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemySpawnConfig MakePaladinConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition);

    //-------------------------------------------------------------
    //! @struct PaladinWeaponAttack
    //! @brief  Paladinが持つ武器1種類ぶんの、攻撃まわりのパラメータ（危険度・エリートの補正前の素の値）
    //! @note   Assets/Prefabs/Enemy/PaladinWeaponAttacks.json が持つ
    //-------------------------------------------------------------
    struct PaladinWeaponAttack {
        WeaponId    weaponId = WeaponId::Warhammer;
        std::string attackClipPath;
        float       attackRange  = 0.0f;    //!< BTが攻撃へ移る距離（＝MoveToPlayerが足を止める距離）
        float       hitboxReach  = 0.0f;    //!< 手ボーンから武器先端までの距離
        float       hitboxRadius = 0.0f;
        float       hitboxDamage = 0.0f;
        float       hitStartTime = 0.0f;
        float       hitDuration  = 0.0f;
    };

    //-------------------------------------------------------------
    //! @brief  武器の種類からPaladinの攻撃パラメータを引く関数
    //! @param  weaponId [in] 武器の種類
    //! @return 対応するパラメータ（見つからなければ表の先頭）
    //! @note   エリートのPaladinが戦闘中に武器を持ち替えるとき（PaladinWeaponSwitchSystem）にも使う
    //-------------------------------------------------------------
    [[nodiscard]]
    const PaladinWeaponAttack& GetPaladinWeaponAttack(WeaponId weaponId);

    //-------------------------------------------------------------
    //! @brief  敵の攻撃モーション・間合い・判定を、持っている武器のものへ書き換える関数
    //! @param  registry    [in,out] エンティティレジストリ
    //! @param  context     [in]     エンジンコンテキスト（攻撃クリップのロードに使う）
    //! @param  enemyEntity [in]     対象の敵
    //! @param  weaponId    [in]     持っている武器の種類
    //! @param  sizeScale   [in]     間合い・判定半径へ掛ける倍率（エリートの大きさ）
    //! @param  damageScale [in]     ダメージへ掛ける倍率（危険度×エリート）
    //! @note   生成時（SpawnBehaviorEnemy）と、エリートの持ち替え（SwitchPaladinWeapon）の両方から呼ぶ
    //-------------------------------------------------------------
    void ApplyHeldWeaponAttack(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                               Tsukino::ECS::Entity enemyEntity, WeaponId weaponId, float sizeScale, float damageScale);
}    // namespace CombatAndroid::ECS
