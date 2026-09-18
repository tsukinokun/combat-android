//-------------------------------------------------------------
//! @file    EliteEnemy.hpp
//! @brief   エリート敵（既存の敵を大きく・硬く・強くした強化個体）の抽選と強化の宣言
//! @note    EnemyDifficultyTableと同じ流儀：生成前の設定（EnemySpawnConfig）へ倍率を掛けるだけで、
//!          エリート専用の行動や生成経路は持たない。数値はEliteEnemy.cppの定数1か所に集めてある
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/EnemySpawnTable.hpp>

#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <hlsl++.h>

#include <random>

namespace Tsukino::EngineIntegration {
    struct EngineContext;
}    // namespace Tsukino::EngineIntegration

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct EnemySpawnConfig;

    //! @brief 同時に居られるエリートの上限
    inline constexpr int kMaxLiveElites = 3;

    //! @brief エリートの押され具合。ノックバックの減衰をこの倍率で速くする（到達距離がおよそ1/倍率になる）
    inline constexpr float kEliteKnockbackDecayScale = 2.0f;

    //! @brief エリートが常に纏う発光の色（紫）。攻撃の予兆（赤）・拾得（シアン）・レベルアップ（金）と混ざらない色
    inline const hlslpp::float3 kEliteGlowColor = hlslpp::float3(0.75f, 0.30f, 1.0f);

    //-------------------------------------------------------------
    //! @brief  今回湧かせる1体をエリートにするかを抽選する
    //! @param  rng            [in,out] 乱数生成器
    //! @param  dangerRank     [in]     現在の危険度
    //! @param  isFinalStretch [in]     ラスト1分か
    //! @param  liveEliteCount [in]     今生きているエリートの数
    //! @return エリートにするならtrue
    //-------------------------------------------------------------
    [[nodiscard]]
    bool RollElite(std::mt19937& rng, int dangerRank, bool isFinalStretch, int liveEliteCount);

    //-------------------------------------------------------------
    //! @brief  生成設定をエリート用に強化する
    //! @param  config [in,out] 危険度の補正（ApplyEnemyDifficulty）を済ませた設定
    //-------------------------------------------------------------
    void ApplyEliteModifiers(EnemySpawnConfig& config);

    //-------------------------------------------------------------
    //! @brief  出現ログに出す敵の呼び名
    //! @param  id [in] 敵の種類
    //-------------------------------------------------------------
    [[nodiscard]]
    const wchar_t* GetEliteDisplayName(EnemyTypeId id);

    //-------------------------------------------------------------
    //! @brief  エリートのPaladinに、手持ちとは別の種類の武器を1〜2本足して、2本以上持たせる
    //! @param  registry    [in,out] ECSレジストリ
    //! @param  context     [in]     エンジンコンテキスト（武器の生成に使う）
    //! @param  rng         [in,out] 足す本数と種類の抽選に使う乱数生成器
    //! @param  enemyEntity [in]     生成直後のPaladin（EnemyHeldWeaponComponentを持つ）
    //! @note   PaladinArsenalComponentを付け、持っている武器を肩の上に並べて浮かせる。
    //!         攻撃ごとの持ち替えはPaladinWeaponSwitchSystemが行う
    //-------------------------------------------------------------
    void EquipPaladinArsenal(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                             std::mt19937& rng, Tsukino::ECS::Entity enemyEntity);

    //-------------------------------------------------------------
    //! @brief  Paladinの持ち武器を差し替える（攻撃モーション・間合い・判定を武器に合わせる）
    //! @param  registry    [in,out] ECSレジストリ
    //! @param  context     [in]     エンジンコンテキスト（攻撃クリップの取得に使う）
    //! @param  enemyEntity [in]     PaladinArsenalComponentを持つPaladin
    //! @param  weaponEntity [in]    持ち替え先（PaladinArsenalComponent::weaponEntitiesのどれか）
    //-------------------------------------------------------------
    void SwitchPaladinWeapon(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                             Tsukino::ECS::Entity enemyEntity, Tsukino::ECS::Entity weaponEntity);
}    // namespace CombatAndroid::ECS
