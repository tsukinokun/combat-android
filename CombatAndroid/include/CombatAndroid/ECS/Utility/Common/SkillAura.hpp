//-------------------------------------------------------------
//! @file   SkillAura.hpp
//! @brief  取得スキルのオーラ（色と強さ）を決める共通処理
//! @note   オーラは輪郭の発光（SkillAuraSystem）と立ちのぼる粒（SkillAuraParticleSystem）の
//!         2本で描く。どちらも「どの罪の色で、どのレベルの強さで光るか」を同じ決め方で
//!         求めないと、体と粒の色が食い違うため、決め方をここに1か所だけ置く
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/Table/SkillTable.hpp>

#include <hlsl++.h>

#include <optional>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct PlayerSkillComponent;    // 前方宣言

    //-------------------------------------------------------------
    //! @struct DominantSkill
    //! @brief  オーラの色を決める「主の罪」
    //-------------------------------------------------------------
    struct DominantSkill {
        SkillId id     = SkillId::Count;    //!< 主の罪
        int     level  = 0;                 //!< その取得段階（1〜そのスキルの最大レベル）
        float   levelT = 0.0f;              //!< レベルを0〜1へ直した値（0=Lv1、1=最大レベル）。強さの補間に使う
    };

    //-------------------------------------------------------------
    //! @brief  オーラの色を決める「主の罪」を選ぶ関数
    //! @param  skills [in] プレイヤーの取得スキル
    //! @return 最もレベルの高いスキル。同率なら最後に取ったもの、それも無ければ並び順で先のもの。
    //!         1つも取っていなければ空
    //-------------------------------------------------------------
    [[nodiscard]]
    std::optional<DominantSkill> ResolveDominantSkill(const PlayerSkillComponent& skills);

    //-------------------------------------------------------------
    //! @brief  スキルの色（Skills.jsonのpanelColor）を彩度を上げて返す関数
    //! @param  id         [in] スキル
    //! @param  saturation [in] 彩度の倍率（1=そのまま、大きいほど鮮やか）
    //! @return 0以上のRGB。panelColorはUI用の落ち着いた色なので、オーラに使うと
    //!         くすむ（明るくするとトーンマップで白へ寄る）。それを打ち消すための調整
    //-------------------------------------------------------------
    [[nodiscard]]
    hlslpp::float3 ResolveSkillAuraColor(SkillId id, float saturation);
}    // namespace CombatAndroid::ECS
