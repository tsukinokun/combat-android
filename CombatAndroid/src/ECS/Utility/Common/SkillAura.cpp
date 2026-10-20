//-------------------------------------------------------------
//! @file   SkillAura.cpp
//! @brief  取得スキルのオーラ（色と強さ）を決める共通処理の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Common/SkillAura.hpp>
#include <CombatAndroid/ECS/Component/Progression/PlayerSkillComponent.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  オーラの色を決める「主の罪」を選ぶ
    //-------------------------------------------------------------
    std::optional<DominantSkill> ResolveDominantSkill(const PlayerSkillComponent& skills) {
        std::optional<SkillId> dominant;
        int                    bestLevel = 0;

        for(int i = 0; i < static_cast<int>(SkillId::Count); ++i) {
            const int level = skills.levels[static_cast<size_t>(i)];
            if(level > bestLevel) {
                bestLevel = level;
                dominant  = static_cast<SkillId>(i);
            }
        }

        if(!dominant)
            return std::nullopt;

        // 同率の首位に最後に取ったスキルが居れば、そちらを優先する
        if(skills.lastAcquired != SkillId::Count && skills.levels[static_cast<size_t>(skills.lastAcquired)] == bestLevel)
            dominant = skills.lastAcquired;

        DominantSkill result;
        result.id     = *dominant;
        result.level  = bestLevel;
        // そのスキルの最大レベルで1になる（最大レベルはスキルごとに違う）
        const int maxLevel = GetSkillMaxLevel(*dominant);
        result.levelT      = (maxLevel > 1) ? std::min(static_cast<float>(bestLevel - 1) / static_cast<float>(maxLevel - 1), 1.0f) : 1.0f;
        return result;
    }

    //-------------------------------------------------------------
    //! @brief  スキルの色を彩度を上げて返す（輝度を保ったまま灰色から遠ざける）
    //-------------------------------------------------------------
    hlslpp::float3 ResolveSkillAuraColor(SkillId id, float saturation) {
        const hlslpp::float3 color = GetSkillEntry(id).panelColor.xyz;
        const float          luma  = hlslpp::dot(color, hlslpp::float3(0.2126f, 0.7152f, 0.0722f));
        const hlslpp::float3 gray(luma, luma, luma);
        return hlslpp::max(gray + (color - gray) * saturation, hlslpp::float3(0.0f, 0.0f, 0.0f));
    }
}    // namespace CombatAndroid::ECS
