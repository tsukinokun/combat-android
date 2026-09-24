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

        //! 文字を載せる左側を暗くする板（Sprite）。背景は3Dの草原（TitleStageSystem）なので、
        //! 全面ではなく文字の下だけを暗くして、右側の武器を見せたままにする。
        //! 操作説明・オプションを開いている間だけは画面全体を覆う
        Tsukino::ECS::Entity backdropEntity = entt::null;

        //! 左側の板から背景へつなぐぼかし用の板（Sprite）。細い帯を並べて1枚ずつ薄くし、
        //! 板の右端が直線で切れて見えないようにする。枚数が少ないと明るい空を背にしたとき
        //! 階段状の縞に見えるため、1段あたりの濃さの差が分からない枚数にしてある
        std::array<Tsukino::ECS::Entity, 40> backdropFadeEntities{};
        Tsukino::ECS::Entity titleEntity    = entt::null;    //!< ゲーム名（Font）
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
