//-------------------------------------------------------------
//! @file   BossHudComponent.hpp
//! @brief  BossHudComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct BossHudComponent
    //! @brief  画面下中央のボス用HUD（名前・HPバー・体勢ゲージ）の部品と表示の状態
    //! @note   BossHudSystemが初回にプレイヤーへ付け、部品もそこで作る。ボス戦の間だけ表示する
    //-------------------------------------------------------------
    struct BossHudComponent {
        Tsukino::ECS::Entity nameEntity       = entt::null;    //!< ボスの呼び名（Font）
        Tsukino::ECS::Entity hpBackEntity     = entt::null;    //!< HPバーの背景
        Tsukino::ECS::Entity hpTrailEntity    = entt::null;    //!< HPバーの「減った分」（残量の後ろで遅れて縮む白い帯）
        Tsukino::ECS::Entity hpFillEntity     = entt::null;    //!< HPバーの残量
        Tsukino::ECS::Entity gaugeFrameEntity = entt::null;    //!< 体勢ゲージの枠（ボスの隙の間だけ金色に光る）
        Tsukino::ECS::Entity gaugeBackEntity  = entt::null;    //!< 体勢ゲージの背景
        Tsukino::ECS::Entity gaugeFillEntity  = entt::null;    //!< 体勢ゲージの溜まり

        Tsukino::ECS::Entity shownBoss       = entt::null;    //!< 表示しているボス（変わったらHPの帯を満タンからやり直す）
        float                trailRatio      = 1.0f;          //!< HPの「減った分」の帯が今示している割合
        float                trailHoldTimer  = 0.0f;          //!< 減ってから帯が縮み始めるまでの残り秒
        float                lastHpRatio     = 1.0f;          //!< 前のフレームのHPの割合（減った瞬間を見分ける）
        float                pulseTime       = 0.0f;          //!< 枠の脈打ち・ダウン中の点滅に使う経過秒
        bool                 wasBroken       = false;         //!< 前のフレームでダウンしていたか（ダウンした瞬間に名前を弾ませる）
    };
}    // namespace CombatAndroid::ECS
