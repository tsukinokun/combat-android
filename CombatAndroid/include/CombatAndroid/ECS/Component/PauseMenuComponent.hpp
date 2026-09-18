//-------------------------------------------------------------
//! @file   PauseMenuComponent.hpp
//! @brief  PauseMenuComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/GameMenu.hpp>
#include <CombatAndroid/ECS/Utility/OptionsMenu.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PauseMenuComponent
    //! @brief  Escで開くポーズメニューの状態。SkillSelectComponentと同じく、
    //!         UIエンティティのハンドルごとプレイヤーエンティティに1つだけ付ける
    //-------------------------------------------------------------
    struct PauseMenuComponent {
        bool isOpen      = false;    //!< メニュー表示中か
        int  cursorIndex = 0;        //!< 選択中の項目

        //! 開いた最初のフレームか。開くのに使ったEscで、そのまま閉じないようにする1フレーム待ち
        bool openedThisFrame = false;

        //! 閉じた後、あと何フレーム停止を続けるか。PauseMenuSystemはPickupSystemより前に走るため、
        //! 「再開」を決定したFを同じフレームの後半で拾得入力として拾わせないための1フレーム
        //!（SkillSelectComponent::closingBlockFramesと同じ理由）
        int closingBlockFrames = 0;

        Tsukino::ECS::Entity backdropEntity = entt::null;    //!< 画面全体を暗くする板（Sprite）
        Tsukino::ECS::Entity titleEntity    = entt::null;    //!< 「PAUSE」の見出し（Font）
        GameMenuWidget       menu;                           //!< 再開／オプション／リトライ／タイトルへ

        OptionsMenuState options;    //!< オプション画面（開いている間は「PAUSE」とメニューを隠す）
    };
}    // namespace CombatAndroid::ECS
