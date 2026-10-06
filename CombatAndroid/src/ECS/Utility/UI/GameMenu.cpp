//-------------------------------------------------------------
//! @file    GameMenu.cpp
//! @brief   縦に並んだ選択肢のメニュー（ポーズ・リザルト・タイトル共通）の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/UI/GameMenu.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/PointerTargetComponent.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 見た目のチューニング値（全て画面ピクセル単位）
        //-------------------------------------------------------------
        constexpr float kItemPitch      = 72.0f;     //!< 選択肢1つぶんの縦の送り
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
        constexpr int kHitLayer       = 25;    //!< マウスの当たり判定。最前面のスプライトにしか反応しないので一番手前に置く

        //! 当たり判定の矩形の色。見えないよう完全に透明にする（スケールは寸法として当たり判定に使われる）
        const hlslpp::float4 kHitRectColor = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.0f);

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

        for(Tsukino::ECS::Entity& hit : widget.hitEntities)
            hit = CreatePointerHitRect(registry, context, sortOrderBase + kHitLayer);

        widget.upPrompt      = CreateKeyPrompt(registry, context, L"W", PromptChevron::Up, sortOrderBase + kPromptLayer);
        widget.downPrompt    = CreateKeyPrompt(registry, context, L"S", PromptChevron::Down, sortOrderBase + kPromptLayer);
        widget.confirmPrompt = CreateKeyPrompt(registry, context, L"F", PromptChevron::Right, sortOrderBase + kPromptLayer);

        return widget;
    }

    //-------------------------------------------------------------
    //! @brief メニューを表示する
    //-------------------------------------------------------------
    void ShowGameMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const GameMenuWidget& widget,
                      float centerX, float topY, std::span<const std::wstring> labels, int cursor, float highlightWidth,
                      bool showConfirmPrompt) {
        const int count = std::min(static_cast<int>(labels.size()), kGameMenuMaxItems);
        if(count <= 0) {
            HideGameMenu(registry, widget);
            return;
        }

        cursor = std::clamp(cursor, 0, count - 1);

        for(int i = 0; i < kGameMenuMaxItems; ++i) {
            if(i >= count) {
                HideUiText(registry, widget.itemEntities[i]);
                HideUiSprite(registry, widget.hitEntities[i]);
                continue;
            }

            const float itemY = topY + kItemPitch * static_cast<float>(i);
            PlaceUiText(registry, widget.itemEntities[i], centerX, itemY, kItemFontScale, labels[i],
                        (i == cursor) ? kSelectedTextColor : kItemTextColor);

            // マウスの当たり判定は強調帯の幅×1段の送り。上下の行と隙間なく接するので、行の間で途切れない
            StretchSprite(registry, context, widget.hitEntities[i], centerX, itemY, highlightWidth, kItemPitch, kHitRectColor);
        }

        const float cursorY = topY + kItemPitch * static_cast<float>(cursor);
        StretchSprite(registry, context, widget.highlightEntity, centerX, cursorY, highlightWidth, kHighlightHeight, kHighlightColor);

        //-------------------------------------------------------------
        // [W][S] は選択中の選択肢の右に縦に並べ、[F] はさらにその右に置く。
        // 選択肢が1つしか無いときは動かす先が無いので [W][S] を出さない
        //-------------------------------------------------------------
        InputPromptStyle style;
        style.scale = kPromptScale;

        const float promptX = centerX + highlightWidth * 0.5f + kPromptGapX;
        if(count > 1) {
            ShowInputPromptAtScreen(registry, context, widget.upPrompt, promptX, cursorY + kPromptUpOffsetY, style);
            ShowInputPromptAtScreen(registry, context, widget.downPrompt, promptX, cursorY + kPromptDownOffsetY, style);
        } else {
            HideInputPrompt(registry, widget.upPrompt);
            HideInputPrompt(registry, widget.downPrompt);
        }
        if(showConfirmPrompt)
            ShowInputPromptAtScreen(registry, context, widget.confirmPrompt, promptX + kPromptConfirmGapX, cursorY, style);
        else
            HideInputPrompt(registry, widget.confirmPrompt);
    }

    //-------------------------------------------------------------
    //! @brief メニューを丸ごと非表示にする
    //-------------------------------------------------------------
    void HideGameMenu(Tsukino::ECS::Registry& registry, const GameMenuWidget& widget) {
        HideUiSprite(registry, widget.highlightEntity);

        for(Tsukino::ECS::Entity item : widget.itemEntities)
            HideUiText(registry, item);

        // 隠したメニューがマウスに反応しないよう、当たり判定も面積ゼロにする
        for(Tsukino::ECS::Entity hit : widget.hitEntities)
            HideUiSprite(registry, hit);

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
    //! @brief このフレームの値の増減を読む
    //-------------------------------------------------------------
    int ReadGameMenuValueStep(const Tsukino::Input::InputSystem& input) {
        int step = 0;
        if(input.IsKeyPressed(Tsukino::Input::KeyCode::A) || input.IsKeyPressed(Tsukino::Input::KeyCode::Left))
            --step;
        if(input.IsKeyPressed(Tsukino::Input::KeyCode::D) || input.IsKeyPressed(Tsukino::Input::KeyCode::Right))
            ++step;
        return step;
    }

    //-------------------------------------------------------------
    //! @brief このフレームに決定が押されたかを読む
    //-------------------------------------------------------------
    bool IsGameMenuConfirmPressed(const Tsukino::Input::InputSystem& input) {
        return input.IsKeyPressed(Tsukino::Input::KeyCode::F) || input.IsKeyPressed(Tsukino::Input::KeyCode::Enter);
    }

    //-------------------------------------------------------------
    //! @brief このフレームのマウス操作を読む
    //-------------------------------------------------------------
    GameMenuPointer ReadGameMenuPointer(Tsukino::ECS::Registry& registry, const Tsukino::Input::InputSystem& input,
                                        const GameMenuWidget& widget) {
        return ReadPointerOverRows(registry, input, widget.hitEntities);
    }

    //-------------------------------------------------------------
    //! @brief マウスの当たり判定に使う透明な矩形を、非表示で作る
    //-------------------------------------------------------------
    Tsukino::ECS::Entity CreatePointerHitRect(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                              int sortOrder) {
        Tsukino::ECS::Entity entity = CreateUiRectEntity(registry, context, sortOrder);
        registry.AddComponent<Tsukino::BuiltIn::ECS::PointerTargetComponent>(entity);
        return entity;
    }

    //-------------------------------------------------------------
    //! @brief 当たり判定の矩形の並びに対するマウス操作を読む
    //-------------------------------------------------------------
    GameMenuPointer ReadPointerOverRows(Tsukino::ECS::Registry& registry, const Tsukino::Input::InputSystem& input,
                                        std::span<const Tsukino::ECS::Entity> rows) {
        GameMenuPointer pointer;

        for(int i = 0; i < static_cast<int>(rows.size()); ++i) {
            const auto* target = registry.try_get<Tsukino::BuiltIn::ECS::PointerTargetComponent>(rows[static_cast<size_t>(i)]);
            if(!target || !target->hovered)
                continue;

            //-------------------------------------------------------------
            // 選択を合わせるのは、マウスが動いたかクリックしたフレームだけ。
            // 止まっているマウスの下の行で毎フレーム上書きすると、W/Sで選んでもすぐ戻されてしまう。
            // メニューを開いた瞬間に、たまたまカーソルがあった行が選ばれることも防げる
            //-------------------------------------------------------------
            Tsukino::i32 deltaX = 0, deltaY = 0;
            input.GetMouseDelta(&deltaX, &deltaY);
            if(deltaX != 0 || deltaY != 0 || target->clicked)
                pointer.hoverIndex = i;

            pointer.clicked = target->clicked;
            break;    // InteractionSystemは最前面の1つにしか立てない
        }

        return pointer;
    }
}    // namespace CombatAndroid::ECS
