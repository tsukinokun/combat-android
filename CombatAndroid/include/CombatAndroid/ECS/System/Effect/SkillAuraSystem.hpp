//-------------------------------------------------------------
//! @file   SkillAuraSystem.hpp
//! @brief  取得スキルの色でプレイヤーの体の輪郭をうっすら光らせるシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  SkillAuraSystem
    //! @brief  取得済みスキルのうち最もレベルの高い罪（同率なら最後に取った方）の色で、
    //!         プレイヤーのRimGlowComponentを光らせる。レベルが上がるほど強く光る。
    //!
    //!         オーラの主役は足元から立ちのぼる光の粒（SkillAuraParticleSystem）で、
    //!         こちらは体に同じ色をうっすら添える脇役。色の決め方は両者で共通
    //!         （Utility/Common/SkillAura.hpp。色はSkills.jsonのpanelColor）。
    //!
    //! @note   RimGlowComponentは溜め攻撃（PlayerAnimationSystem）とレベルアップ後無敵
    //!         （SkillSelectSystem）も使う。オーラはその2つより弱い「普段の状態」なので、
    //!         PlayerComponent::isCharging / levelUpInvincibleTimer を見て、その間は書き込まない。
    //!         あちらが終わるフレームに active=false を書いても、このSystemが同じフレームの
    //!         後ろで書き直すので、オーラが一瞬消えることはない（実行順はSystemPriority::SkillAura）
    //-------------------------------------------------------------
    class SkillAuraSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
