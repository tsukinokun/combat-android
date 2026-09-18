//-------------------------------------------------------------
//! @file    WeaponEvolutionTable.hpp
//! @brief   武器の進化（最大レベルの武器＋対応する大罪スキルで性能が変わる）の表の宣言
//! @note    WeaponTable/SkillTableと同じ流儀：「何が何に進化するか」の数値だけをここに集め、
//!          進化の効果はWeaponComponentに既にある値（範囲・射程・貫通など）の書き換えだけで表す。
//!          新しい攻撃処理は持たない
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/SkillTable.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

#include <Tsukino/Core/ECS/Registry/Registry.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct WeaponComponent;

    //-------------------------------------------------------------
    //! @struct WeaponEvolutionEntry
    //! @brief  武器1種類ぶんの進化の条件と効果
    //! @note   倍率は1.0で「変えない」、貫通段階は0で「変えない」
    //-------------------------------------------------------------
    struct WeaponEvolutionEntry {
        WeaponId       weaponId;              //!< 進化する武器（表の並びの確認用）
        SkillId        requiredSkill;         //!< 条件になる大罪スキル
        int            requiredSkillLevel;    //!< そのスキルに要るレベル（武器側は常にkMaxWeaponLevel）
        const wchar_t* displayName;           //!< 進化後の名前（取得ログ・スキルカードの案内に出す）

        float damageScale;              //!< 基礎ダメージの倍率（RecalculateWeaponStatsが掛ける）
        float areaRadiusScale;          //!< 範囲攻撃（3段目）の半径の倍率
        float areaKnockbackSpeed;       //!< 範囲攻撃の吹っ飛ばし初速。元の値より大きいときだけ上書きする
        float areaKnockbackStun;        //!< 範囲攻撃の追加スタン。同上
        float rangeScale;               //!< 刃の届く距離（当たり判定カプセルの長さ）の倍率
        float hitCapsuleRadiusScale;    //!< 刃の太さの倍率
        int   projectilePierceMinChargeStage;    //!< 斬撃弾が貫通し始める溜め段階（0なら変えない）
        float projectileRadiusScale;    //!< 斬撃弾の当たり半径の倍率
        float projectileDistanceScale;  //!< 斬撃弾の飛距離（最大距離・寿命）の倍率
    };

    //-------------------------------------------------------------
    //! @brief  武器の種類から進化の設定を引く
    //! @param  id [in] 武器の識別子
    //! @return 対応する設定（表はWeaponIdの並び順に定義されている）
    //-------------------------------------------------------------
    [[nodiscard]]
    const WeaponEvolutionEntry& GetWeaponEvolution(WeaponId id);

    //-------------------------------------------------------------
    //! @brief  武器を進化させる（evolvedを立て、範囲・射程・貫通などを書き換える）
    //! @param  weapon [in,out] 対象の武器。既に進化済みなら何もしない
    //-------------------------------------------------------------
    void ApplyWeaponEvolution(WeaponComponent& weapon);

    //-------------------------------------------------------------
    //! @brief  プレイヤーの持つ武器のうち、条件を満たしたものを進化させる
    //! @param  registry     [in,out] ECSレジストリ
    //! @param  playerEntity [in]     プレイヤー（PlayerComponent / PlayerSkillComponentを持つ）
    //! @note   スキル取得・武器レベルアップ・武器の新規取得の直後に呼ぶ。
    //!         進化した武器ごとに取得ログ（WeaponEvolved）を流し、長めの発光を焼く
    //-------------------------------------------------------------
    void TryEvolvePlayerWeapons(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity playerEntity);
}    // namespace CombatAndroid::ECS
