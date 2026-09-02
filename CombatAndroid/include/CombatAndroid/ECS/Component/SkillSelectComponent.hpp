//-------------------------------------------------------------
//! @file   SkillSelectComponent.hpp
//! @brief  SkillSelectComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <array>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct SkillSelectCardEntities
    //! @brief  スキルカード1枚を構成するエンティティ
    //-------------------------------------------------------------
    struct SkillSelectCardEntities {
        Tsukino::ECS::Entity panelEntity = entt::null;    //!< 背景パネル（Sprite）
        Tsukino::ECS::Entity nameEntity  = entt::null;    //!< スキル名（Font）
        Tsukino::ECS::Entity descEntity  = entt::null;    //!< 効果の説明文（Font）
    };

    //-------------------------------------------------------------
    //! @struct SkillSelectComponent
    //! @brief  レベルアップ時のスキル選択メニューの状態。
    //!         GameOverComponent / PlayerHudComponentと同じく、UIエンティティのハンドルごと
    //!         プレイヤーエンティティに1つだけ付ける
    //-------------------------------------------------------------
    struct SkillSelectComponent {
        bool isActive        = false;    //!< メニュー表示中か
        int  pendingLevelUps = 0;        //!< 未消化のレベルアップ回数（ExpOrbSystemが積む）
        int  cursorIndex     = 0;        //!< 選択中のカード（0 〜 candidateCount-1）
        int  candidateCount  = 0;        //!< 今回提示している選択肢の数（カンストで3つ揃わないことがある）

        std::array<SkillId, kSkillChoiceMax> candidates{};    //!< 今回提示しているスキル

        //! メニューを開いた最初のフレームか。表示した瞬間のフレームで
        //! そのまま決定入力を拾わないようにするための1フレーム待ち（GameOverSystemと同じ流儀）
        bool openedThisFrame = false;

        //! 決定した後、あと何フレーム入力の遮断を続けるか。
        //! SkillSelectSystemはPickupSystem等より前の優先度で走るため、決定したフレームの
        //! isActive=falseをそのままにすると同じフレームの後半でPickupSystemが
        //! 「決定に使ったF」を拾得入力として拾ってしまう。それを潰すための1フレーム
        int closingBlockFrames = 0;

        Tsukino::ECS::Entity backdropEntity  = entt::null;    //!< 画面全体を暗くする板（Sprite）
        Tsukino::ECS::Entity highlightEntity = entt::null;    //!< 選択中カードの強調枠（Sprite）
        Tsukino::ECS::Entity titleEntity     = entt::null;    //!< 「LEVEL UP!」テキスト（Font）

        std::array<SkillSelectCardEntities, kSkillChoiceMax> cards{};    //!< カード3枚ぶんのエンティティ
    };
}    // namespace CombatAndroid::ECS
