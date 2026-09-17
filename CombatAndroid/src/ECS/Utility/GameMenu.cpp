//-------------------------------------------------------------
//! @file    GameMenu.cpp
//! @brief   縦に並んだ選択肢のメニュー（ポーズ・リザルト・タイトル共通）の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/GameMenu.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 見た目のチューニング値（全て画面ピクセル単位）
        //-------------------------------------------------------------
        constexpr float kItemPitch      = 72.0f;     //!< 選択肢1つぶんの縦の送り
        constexpr float kHighlightWidth = 440.0f;    //!< 強調帯の幅
        constexpr float kHighlightHeight = 56.0f;    //!< 強調帯の高さ
        constexpr float kItemFontScale  = 1.25f;     //!< 選択肢の文字の大きさ

        constexpr float kPromptGapX        = 80.0f;     //!< 強調帯の右端からプロンプトの列までの距離
        constexpr float kPromptUpOffsetY   = -40.0f;    //!< 選択中の選択肢から見た[W]のY
        constexpr float kPromptDownOffsetY = 40.0f;     //!< 同じく[S]のY
        constexpr float kPromptConfirmGapX = 120.0f;    //!< [W][S]の列から[F]までの横の距離
        constexpr float kPromptScale       = 1.0f;

        const hlslpp::float4 kHighlightColor    = hlslpp::float4(1.0f, 0.92f, 0.35f, 0.92f);    //!< スキル選択の強調枠と同じ黄色
        const hlslpp::float4 kSelectedTextColor = hlslpp::float4(0.08f, 0.08f, 0.10f, 1.0f);    //!< 黄色の帯の上なので濃い色
        const hlslpp::float4 kItemTextColor     = hlslpp::float4(0.95f, 0.95f, 0.95f, 1.0f);

        //! 帯・文字・プロンプトが基準から使う層のずらし量
        constexpr int kHighlightLayer = 0;
        constexpr int kItemLayer      = 10;
        constexpr int kPromptLayer    = 20;

        //-------------------------------------------------------------
        //! @brief  キーキャップのプロンプトを1つ作る
        //! @param  registry  [in] ECSレジストリ
        //! @param  context   [in] エンジンコンテキスト
        //! @param  keyLabel  [in] キーの文字
        //! @param  chevron   [in] 添える矢印
        //! @param  sortOrder [in] 描画層の基準
        //! @return 作ったウィジェット
        //-------------------------------------------------------------
        [[nodiscard]]
        InputPromptWidget CreateKeyPrompt(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                          const wchar_t* keyLabel, PromptChevron chevron, int sortOrder) {
            InputPromptDesc desc;
            desc.keyLabel      = keyLabel;
            desc.chevron       = chevron;
            desc.sortOrderBase = sortOrder;
            return CreateInputPromptWidget(registry, context, desc);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief メニューのエンティティ一式を非表示で作る
    //-------------------------------------------------------------
    GameMenuWidget CreateGameMenuWidget(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                        int sortOrderBase) {
        GameMenuWidget widget;

        widget.highlightEntity = CreateUiRectEntity(registry, context, sortOrderBase + kHighlightLayer);

        for(Tsukino::ECS::Entity& item : widget.itemEntities)
            item = CreateUiTextEntity(registry, sortOrderBase + kItemLayer, UiTextAlign::Center);

        widget.upPrompt      = CreateKeyPrompt(registry, context, L"W", PromptChevron::Up, sortOrderBase + kPromptLayer);
        widget.downPrompt    = CreateKeyPrompt(registry, context, L"S", PromptChevron::Down, sortOrderBase + kPromptLayer);
        widget.confirmPrompt = CreateKeyPrompt(registry, context, L"F", PromptChevron::Right, sortOrderBase + kPromptLayer);

        return widget;
    }

    //-------------------------------------------------------------
    //! @brief メニューを表示する
    //-------------------------------------------------------------
    void ShowGameMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const GameMenuWidget& widget,
                      float centerX, float topY, std::span<const std::wstring> labels, int cursor) {
        const int count = std::min(static_cast<int>(labels.size()), kGameMenuMaxItems);
        if(count <= 0) {
            HideGameMenu(registry, widget);
            return;
        }

        cursor = std::clamp(cursor, 0, count - 1);

        for(int i = 0; i < kGameMenuMaxItems; ++i) {
            if(i >= count) {
                HideUiText(registry, widget.itemEntities[i]);
                continue;
            }

            const float itemY = topY + kItemPitch * static_cast<float>(i);
            PlaceUiText(registry, widget.itemEntities[i], centerX, itemY, kItemFontScale, labels[i],
                        (i == cursor) ? kSelectedTextColor : kItemTextColor);
        }

        const float cursorY = topY + kItemPitch * static_cast<float>(cursor);
        StretchSprite(registry, context, widget.highlightEntity, centerX, cursorY, kHighlightWidth, kHighlightHeight, kHighlightColor);

        //-------------------------------------------------------------
        // [W][S] は選択中の選択肢の右に縦に並べ、[F] はさらにその右に置く。
        // 選択肢が1つしか無いときは動かす先が無いので [W][S] を出さない
        //-------------------------------------------------------------
        InputPromptStyle style;
        style.scale = kPromptScale;

        const float promptX = centerX + kHighlightWidth * 0.5f + kPromptGapX;
        if(count > 1) {
            ShowInputPromptAtScreen(registry, context, widget.upPrompt, promptX, cursorY + kPromptUpOffsetY, style);
            ShowInputPromptAtScreen(registry, context, widget.downPrompt, promptX, cursorY + kPromptDownOffsetY, style);
        } else {
            HideInputPrompt(registry, widget.upPrompt);
            HideInputPrompt(registry, widget.downPrompt);
        }
        ShowInputPromptAtScreen(registry, context, widget.confirmPrompt, promptX + kPromptConfirmGapX, cursorY, style);
    }

    //-------------------------------------------------------------
    //! @brief メニューを丸ごと非表示にする
    //-------------------------------------------------------------
    void HideGameMenu(Tsukino::ECS::Registry& registry, const GameMenuWidget& widget) {
        HideUiSprite(registry, widget.highlightEntity);

        for(Tsukino::ECS::Entity item : widget.itemEntities)
            HideUiText(registry, item);

        HideInputPrompt(registry, widget.upPrompt);
        HideInputPrompt(registry, widget.downPrompt);
        HideInputPrompt(registry, widget.confirmPrompt);
    }

    //-------------------------------------------------------------
    //! @brief このフレームのカーソル移動量を読む
    //-------------------------------------------------------------
    int ReadGameMenuStep(const Tsukino::Input::InputSystem& input) {
        int step = 0;
        if(input.IsKeyPressed(Tsukino::Input::KeyCode::W) || input.IsKeyPressed(Tsukino::Input::KeyCode::Up))
            --step;
        if(input.IsKeyPressed(Tsukino::Input::KeyCode::S) || input.IsKeyPressed(Tsukino::Input::KeyCode::Down))
            ++step;

        // ホイールは1ノッチ=±1.0。上へ回す＝上の選択肢へ（スキル選択と同じ向き）
        const float wheelDelta = input.GetWheelDelta();
        if(wheelDelta > 0.0f)
            --step;
        else if(wheelDelta < 0.0f)
            ++step;

        return std::clamp(step, -1, 1);
    }

    //-------------------------------------------------------------
    //! @brief このフレームに決定が押されたかを読む
    //-------------------------------------------------------------
    bool IsGameMenuConfirmPressed(const Tsukino::Input::InputSystem& input) {
        return input.IsKeyPressed(Tsukino::Input::KeyCode::F) || input.IsKeyPressed(Tsukino::Input::KeyCode::Enter);
    }
}    // namespace CombatAndroid::ECS
