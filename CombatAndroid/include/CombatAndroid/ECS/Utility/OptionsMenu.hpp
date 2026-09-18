//-------------------------------------------------------------
//! @file    OptionsMenu.hpp
//! @brief   オプション画面（感度・音量・画面揺れ）の部品の宣言
//! @note    タイトル画面とポーズメニューの両方から開くので、システムにはせず
//!          GameMenuと同じ「呼び出し側のコンポーネントに埋め込む部品」にしている。
//!          開いている間は呼び出し側が自分のメニューを隠し、入力をこちらへ回す
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/GameMenu.hpp>
#include <CombatAndroid/ECS/Utility/InputPromptWidget.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct OptionsMenuState
    //! @brief  オプション画面の状態とUIエンティティ
    //-------------------------------------------------------------
    struct OptionsMenuState {
        bool isOpen      = false;    //!< 表示中か
        int  cursorIndex = 0;        //!< 選択中の項目

        //! 開いた最初のフレームか。開くのに使った決定入力を、そのまま拾わないための1フレーム待ち
        bool openedThisFrame = false;

        //! 値を変えたか。閉じるときにだけ保存するための印
        bool changed = false;

        //! 開いたときのBGM音量。閉じるときに変わっていればBGMを鳴らし直す
        //!（鳴らし直すと曲が頭に戻るので、変えていないときは触らない）
        int bgmVolumeAtOpen = 100;

        Tsukino::ECS::Entity panelEntity  = entt::null;    //!< 項目の後ろに敷く板（Sprite）
        Tsukino::ECS::Entity headerEntity = entt::null;    //!< 「オプション」の見出し（Font）
        Tsukino::ECS::Entity hintEntity   = entt::null;    //!< 操作の説明（Font）
        GameMenuWidget       menu;                         //!< 各項目ともどる
        InputPromptWidget    decreasePrompt;               //!< [A] 値を下げる
        InputPromptWidget    increasePrompt;               //!< [D] 値を上げる
    };

    //-------------------------------------------------------------
    //! @brief  オプション画面のエンティティ一式を非表示で作る
    //! @param  registry      [in] ECSレジストリ
    //! @param  context       [in] エンジンコンテキスト
    //! @param  sortOrderBase [in] 描画層の基準（+0〜+40を使う）
    //! @return 作った状態
    //-------------------------------------------------------------
    [[nodiscard]]
    OptionsMenuState CreateOptionsMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                       int sortOrderBase);

    //-------------------------------------------------------------
    //! @brief  オプション画面を開く
    //! @param  registry [in] ECSレジストリ
    //! @param  context  [in] エンジンコンテキスト
    //! @param  options  [in,out] 対象
    //-------------------------------------------------------------
    void OpenOptionsMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, OptionsMenuState& options);

    //-------------------------------------------------------------
    //! @brief  開いているオプション画面の入力を処理する（毎フレーム呼ぶ）
    //! @param  registry [in] ECSレジストリ
    //! @param  context  [in] エンジンコンテキスト
    //! @param  options  [in,out] 対象
    //! @return このフレームで閉じたらtrue（呼び出し側は自分のメニューを表示し直す）
    //! @note   W/Sで項目、A/Dで値、F（もどる）・Escで閉じる。閉じるときに設定を保存し、
    //!         BGMを新しい音量で鳴らし直す
    //-------------------------------------------------------------
    bool UpdateOptionsMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, OptionsMenuState& options);

    //-------------------------------------------------------------
    //! @brief  オプション画面を隠す（閉じた状態にはしない。シーンを離れるとき等に使う）
    //! @param  registry [in] ECSレジストリ
    //! @param  options  [in] 対象
    //-------------------------------------------------------------
    void HideOptionsMenu(Tsukino::ECS::Registry& registry, const OptionsMenuState& options);
}    // namespace CombatAndroid::ECS
