//-------------------------------------------------------------
//! @file    GameMenu.hpp
//! @brief   縦に並んだ選択肢のメニュー（ポーズ・リザルト・タイトル共通）の宣言
//! @note    スキル選択と同じ操作（W/Sで選ぶ・Fで決定、またはマウスを重ねて左クリック）と見た目のキーキャップを、
//!          画面ごとに書き直さずに済むよう1つにまとめたもの。
//!          選択肢の文字・強調帯・[W][S][F]のプロンプトだけを受け持ち、
//!          暗転板や見出しは各画面が持つ
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/InputPromptWidget.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <array>
#include <span>
#include <string>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}
namespace Tsukino::Input {
    class InputSystem;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! 1つのメニューに並べられる選択肢の最大数（オプション画面の4項目＋もどる）
    inline constexpr int kGameMenuMaxItems = 5;

    //! 選択中の選択肢の後ろに敷く帯の既定の幅
    inline constexpr float kGameMenuDefaultHighlightWidth = 440.0f;

    //-------------------------------------------------------------
    //! @struct GameMenuWidget
    //! @brief  メニュー1つぶんのエンティティ束。生成直後は全て非表示
    //-------------------------------------------------------------
    struct GameMenuWidget {
        Tsukino::ECS::Entity                                  highlightEntity = entt::null;    //!< 選択中の選択肢の後ろに敷く帯（Sprite）
        std::array<Tsukino::ECS::Entity, kGameMenuMaxItems>   itemEntities{};                  //!< 選択肢の文字（Font）
        InputPromptWidget                                     upPrompt;                        //!< [W] カーソル上
        InputPromptWidget                                     downPrompt;                      //!< [S] カーソル下
        InputPromptWidget                                     confirmPrompt;                   //!< [F] 決定
        std::array<Tsukino::ECS::Entity, kGameMenuMaxItems>   hitEntities{};                   //!< 行ごとのマウス当たり判定（透明な矩形＋PointerTargetComponent）
    };

    //-------------------------------------------------------------
    //! @struct GameMenuPointer
    //! @brief  このフレームのマウスによるメニュー操作
    //-------------------------------------------------------------
    struct GameMenuPointer {
        int  hoverIndex = -1;       //!< カーソルを合わせる選択肢（マウスが動いたかクリックしたフレームだけ。無ければ-1）
        bool clicked    = false;    //!< 選択肢の上で左クリックした（＝決定）
    };

    //-------------------------------------------------------------
    //! @brief  メニューのエンティティ一式を非表示で作る
    //! @param  registry      [in] ECSレジストリ
    //! @param  context       [in] エンジンコンテキスト
    //! @param  sortOrderBase [in] 重なり順の基準。帯=+0、文字=+10、プロンプト=+20〜+24、マウスの当たり判定=+25を使う
    //! @return 生成したエンティティ束
    //-------------------------------------------------------------
    [[nodiscard]]
    GameMenuWidget CreateGameMenuWidget(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                        int sortOrderBase);

    //-------------------------------------------------------------
    //! @brief  メニューを表示する
    //! @param  registry [in] ECSレジストリ
    //! @param  context  [in] エンジンコンテキスト
    //! @param  widget   [in] 対象のメニュー
    //! @param  centerX  [in] 選択肢の中心のスクリーンX
    //! @param  topY     [in] 1つ目の選択肢の中心のスクリーンY
    //! @param  labels   [in] 選択肢の文字（kGameMenuMaxItems個まで。超えた分は出さない）
    //! @param  cursor   [in] 選択中の選択肢
    //! @param  highlightWidth   [in] 強調帯の幅（選択肢の文字が長いメニューで広げる）
    //! @param  showConfirmPrompt [in] [F]の決定案内を出すか。falseなら隠す
    //!         （A/Dだけで値を変える項目のように、選択中の行でFが何もしない場合に使う）
    //-------------------------------------------------------------
    void ShowGameMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const GameMenuWidget& widget,
                      float centerX, float topY, std::span<const std::wstring> labels, int cursor,
                      float highlightWidth = kGameMenuDefaultHighlightWidth, bool showConfirmPrompt = true);

    //-------------------------------------------------------------
    //! @brief  メニューを丸ごと非表示にする
    //! @param  registry [in] ECSレジストリ
    //! @param  widget   [in] 対象のメニュー
    //-------------------------------------------------------------
    void HideGameMenu(Tsukino::ECS::Registry& registry, const GameMenuWidget& widget);

    //-------------------------------------------------------------
    //! @brief  このフレームのカーソル移動量を読む
    //! @param  input [in] 入力
    //! @return -1: 上へ、+1: 下へ、0: 動かさない
    //! @note   W/S・上下キー・マウスホイールを同じ「1段」として扱う（スキル選択と同じ）
    //-------------------------------------------------------------
    [[nodiscard]]
    int ReadGameMenuStep(const Tsukino::Input::InputSystem& input);

    //-------------------------------------------------------------
    //! @brief  このフレームの値の増減を読む（オプション画面の左右）
    //! @param  input [in] 入力
    //! @return -1（A・左キー）／+1（D・右キー）／0
    //-------------------------------------------------------------
    [[nodiscard]]
    int ReadGameMenuValueStep(const Tsukino::Input::InputSystem& input);

    //-------------------------------------------------------------
    //! @brief  このフレームに決定が押されたかを読む
    //! @param  input [in] 入力
    //! @return true: F か Enter が押された
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsGameMenuConfirmPressed(const Tsukino::Input::InputSystem& input);

    //-------------------------------------------------------------
    //! @brief  このフレームのマウス操作を読む
    //! @param  registry [in] ECSレジストリ
    //! @param  input    [in] 入力
    //! @param  widget   [in] 対象のメニュー
    //! @return カーソルを合わせる選択肢と、クリックで決定したか
    //! @note   重なりの判定はエンジンのInteractionSystemがPointerTargetComponentへ書いたものを読む。
    //!         選択肢へカーソルを合わせるのは、マウスが動いたかクリックしたフレームだけ。
    //!         止まっているマウスがW/Sの選択を毎フレーム上書きしないようにするため
    //-------------------------------------------------------------
    [[nodiscard]]
    GameMenuPointer ReadGameMenuPointer(Tsukino::ECS::Registry& registry, const Tsukino::Input::InputSystem& input,
                                        const GameMenuWidget& widget);

    //-------------------------------------------------------------
    //! @brief  マウスの当たり判定に使う透明な矩形を、非表示で作る
    //! @param  registry  [in] ECSレジストリ
    //! @param  context   [in] エンジンコンテキスト
    //! @param  sortOrder [in] 描画層。マウスは最前面のスプライトにだけ反応するので、
    //!                   反応させたい見た目（帯・カード）より手前に置く
    //! @return 作ったエンティティ（PointerTargetComponent付き）
    //! @note   配置はStretchSpriteでアルファ0の色を渡して行い、隠すときはHideUiSprite
    //-------------------------------------------------------------
    [[nodiscard]]
    Tsukino::ECS::Entity CreatePointerHitRect(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                              int sortOrder);

    //-------------------------------------------------------------
    //! @brief  当たり判定の矩形の並び（上から順の選択肢）に対するマウス操作を読む
    //! @param  registry [in] ECSレジストリ
    //! @param  input    [in] 入力
    //! @param  rows     [in] 選択肢ごとの当たり判定の矩形（CreatePointerHitRectで作ったもの）
    //! @return カーソルを合わせる選択肢と、クリックで決定したか
    //! @note   GameMenu以外の選択画面（スキル選択のカード）からも使う
    //-------------------------------------------------------------
    [[nodiscard]]
    GameMenuPointer ReadPointerOverRows(Tsukino::ECS::Registry& registry, const Tsukino::Input::InputSystem& input,
                                        std::span<const Tsukino::ECS::Entity> rows);

}    // namespace CombatAndroid::ECS
