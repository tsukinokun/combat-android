//-------------------------------------------------------------
//! @file    EliteEnemy.hpp
//! @brief   エリート敵（既存の敵を大きく・硬く・強くした強化個体）の抽選と強化の宣言
//! @note    EnemyDifficultyTableと同じ流儀：生成前の設定（EnemySpawnConfig）へ倍率を掛けるだけで、
//!          エリート専用の行動や生成経路は持たない。数値は Assets/Tables/Elite.json が持ち、初回参照で1度だけ読む
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/EnemySpawnTable.hpp>

#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <hlsl++.h>

#include <array>
#include <random>
#include <string>

namespace Tsukino::EngineIntegration {
    struct EngineContext;
}    // namespace Tsukino::EngineIntegration

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct EnemySpawnConfig;

    //-------------------------------------------------------------
    //! @struct EliteSettings
    //! @brief  エリートの出現率・強化の倍率・見た目の設定（Assets/Tables/Elite.json）
    //! @note   怯み閾値の倍率（thresholdScale）は体力の倍率（healthScale）以下に保つこと
    //!         （EnemyDifficultyTableと同じ約束。読み込み時に検査してLog::Errorを出す）
    //-------------------------------------------------------------
    struct EliteSettings {
        int   maxLiveElites           = 3;        //!< 同時に居られるエリートの上限
        int   firstEliteRank          = 2;        //!< この危険度から出始める
        float chanceAtFirstRank       = 0.0f;     //!< 出始めの確率
        float chancePerRank           = 0.0f;     //!< 危険度1ごとに増える確率
        float chanceMax               = 0.0f;     //!< 確率の上限
        float finalStretchChanceScale = 1.0f;     //!< ラスト1分の倍率

        float sizeScale      = 1.0f;    //!< 見た目・体の当たり・攻撃範囲
        float healthScale    = 1.0f;    //!< 体力
        float damageScale    = 1.0f;    //!< 攻撃力
        float thresholdScale = 1.0f;    //!< 怯み閾値
        float moveSpeedScale = 1.0f;    //!< 移動速度
        float expRewardScale = 1.0f;    //!< 経験値

        //! エリートの押され具合。ノックバックの減衰をこの倍率で速くする（到達距離がおよそ1/倍率になる）
        float knockbackDecayScale = 1.0f;

        //! エリートが常に纏う発光の色。攻撃の予兆（赤）・拾得（シアン）・レベルアップ（金）と混ざらない色にする
        hlslpp::float3 glowColor = hlslpp::float3(1.0f, 1.0f, 1.0f);

        float arsenalSpacing = 80.0f;     //!< エリートPaladinが並べて浮かせる武器の、横の間隔
        float arsenalHeight  = 170.0f;    //!< 同、普通の大きさのときの浮遊の高さ（大きさの倍率を掛けて使う）
        float arsenalDepth   = -30.0f;    //!< 同、前後（少し背中側）

        //! 出現ログに出す敵の呼び名（EnemyTypeIdの並び順。JSONのキーは敵の名前）
        std::array<std::wstring, static_cast<size_t>(EnemyTypeId::Count)> displayNames{};
    };

    //-------------------------------------------------------------
    //! @brief  エリートの設定を得る関数
    //! @return 設定（初回の呼び出しで Assets/Tables/Elite.json を1度だけ読む）
    //-------------------------------------------------------------
    [[nodiscard]]
    const EliteSettings& GetEliteSettings();

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
    const std::wstring& GetEliteDisplayName(EnemyTypeId id);

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
