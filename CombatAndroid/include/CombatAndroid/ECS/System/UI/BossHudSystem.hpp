//-------------------------------------------------------------
//! @file   BossHudSystem.hpp
//! @brief  ボス戦の間、画面下中央にボスの名前・HPバー・体勢ゲージを出すシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  BossHudSystem
    //! @brief  ボス戦の間だけ、画面下中央にボスの名前・HPバー・体勢ゲージ（ブレイク）を出す。
    //!         体勢ゲージは溜まるほど白から橙へ変わり、ダウン中は点滅しながら残り時間に合わせて減る。
    //!         ボスの隙（技の後・ダウン中）の間は、ゲージの枠を金色に脈打たせて攻め時を知らせる（文字は出さない）
    //! @note   部品はBossHudComponentに持たせ、初回にプレイヤーへ付ける。
    //!         HP（CombatSystem）と体勢ゲージ（ApplyBossHitModifiers・BossBehavior）が確定した後に読む（SystemPriority::BossHud）
    //-------------------------------------------------------------
    class BossHudSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief システムの更新
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
