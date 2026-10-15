//-------------------------------------------------------------
//! @file   BossProgressComponent.hpp
//! @brief  BossProgressComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <entt/entt.hpp>

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum  BossPhase
    //! @brief 走行の山場の進み具合（並び順どおりに進み、戻らない）
    //-------------------------------------------------------------
    enum class BossPhase : int {
        BeforeMid = 0,    //!< 中ボスが出るまで（走行の時計で数える）
        MidActive,        //!< 中ボスと戦っている
        Interlude,        //!< 中ボスを倒してからラスボスが出るまで
        FinalActive,      //!< ラスボスと戦っている
        FinalDefeated,    //!< ラスボスを倒した（RunResultSystemがクリアにする）
    };

    //-------------------------------------------------------------
    //! @struct BossProgressComponent
    //! @brief  中ボス → ラスボス → クリアの進行状態。プレイヤーエンティティに1つだけ付け、
    //!         BossDirectorSystemだけが書き込む
    //! @note   リトライはシーンの作り直しなので、既定値がそのまま「走行のリセット」になる
    //!         （RunClockComponentと同じ割り切り）
    //-------------------------------------------------------------
    struct BossProgressComponent {
        BossPhase            phase        = BossPhase::BeforeMid;
        float                phaseElapsed = 0.0f;           //!< ボスを倒した後の待ちの経過秒（ゲーム内時間）
        float                interludeStartSeconds = 0.0f;  //!< 中ボスを倒し終えた時点の走行の時計（ラスボスまではこの時計で数える）
        Tsukino::ECS::Entity bossEntity   = entt::null;     //!< 戦っているボス
        bool                 bossDown     = false;          //!< 戦っているボスのHPが0になったか（撃破の演出待ちに入った）

        //-----------------------------------------------------
        // 出現の告知（画面中央の斜めの帯と、大きな呼び名）。シーン初期化時に非表示で作る
        //-----------------------------------------------------
        Tsukino::ECS::Entity announceBandEntity     = entt::null;
        Tsukino::ECS::Entity announceTitleEntity    = entt::null;
        float                announceElapsed        = -1.0f;    //!< 告知を出してからの経過秒（実時間）。負なら出していない
        std::wstring         announceTitle;                     //!< 出しているボスの呼び名（例：「夜の聖騎士」）
        bool                 announceFinal = false;             //!< ラスボスの告知か（色を変える）
    };
}    // namespace CombatAndroid::ECS
