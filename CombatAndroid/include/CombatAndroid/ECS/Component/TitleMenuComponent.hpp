//-------------------------------------------------------------
//! @file   TitleMenuComponent.hpp
//! @brief  TitleMenuComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/GameMenu.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <CombatAndroid/ECS/Utility/OptionsMenu.hpp>

#include <array>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! 操作説明に並べる行数
    inline constexpr int kTitleControlsLineCount = 9;

    //-------------------------------------------------------------
    //! @struct TitleMenuComponent
    //! @brief  タイトル画面（TitleScene）の状態とUIエンティティ。
    //!         タイトル画面に1つだけ置く目印のエンティティに付ける
    //-------------------------------------------------------------
    struct TitleMenuComponent {
        int  cursorIndex     = 0;        //!< 選択中の項目（はじめる／操作説明／オプション／終了）
        bool showingControls = false;    //!< 操作説明を開いているか

        //! 画面の状態を切り替えた最初のフレームか。切り替えに使った決定入力を、
        //! 切り替えた先の画面でそのまま拾わないようにする1フレーム待ち
        bool changedThisFrame = true;

        //! 画面全体を覆う背景の板（Sprite）。戦闘シーンから戻ってきたとき、Rendererに残った
        //! 空の設定で背景に空が描かれてしまうため、起動直後と同じ見た目になるよう不透明な板で覆う
        Tsukino::ECS::Entity backdropEntity = entt::null;
        Tsukino::ECS::Entity titleEntity    = entt::null;    //!< ゲーム名（Font）
        Tsukino::ECS::Entity subtitleEntity = entt::null;    //!< 副題（Font）
        Tsukino::ECS::Entity bestEntity     = entt::null;    //!< ベスト記録（Font）
        GameMenuWidget       menu;                           //!< はじめる／操作説明／終了

        Tsukino::ECS::Entity                                     controlsPanelEntity  = entt::null;    //!< 操作説明の板（Sprite）
        Tsukino::ECS::Entity                                     controlsHeaderEntity = entt::null;    //!< 「操作説明」（Font）
        std::array<Tsukino::ECS::Entity, kTitleControlsLineCount> controlsActionEntities{};              //!< 操作の名前（Font）
        std::array<Tsukino::ECS::Entity, kTitleControlsLineCount> controlsKeyEntities{};                 //!< 割り当てられたキー（Font）
        GameMenuWidget                                           controlsMenu;                          //!< もどる

        OptionsMenuState options;    //!< オプション画面（開いている間はタイトルの文字とメニューを隠す）
    };
}    // namespace CombatAndroid::ECS
