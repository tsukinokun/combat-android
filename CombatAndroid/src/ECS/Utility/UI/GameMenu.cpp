//-------------------------------------------------------------
//! @file    GameMenu.cpp
//! @brief   縦に並んだ選択肢のメニュー（ポーズ・リザルト・タイトル共通）の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/UI/GameMenu.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/PointerTargetComponent.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

#include <algorithm>
#include <numbers>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct GameMenuParams
        //! @brief  見た目のチューニング値（Assets/Tables/Systems/GameMenu.json。ここの初期値はJSONにキーが無いときの既定値）
        //! @note   全て画面ピクセル単位。選択肢を縦一直線・同じ大きさで並べると単調になるので、
        //!         行ごとに右へずらして斜めの段にし、選択中の行だけ大きく・帯付きで見せる
        //-------------------------------------------------------------
        struct GameMenuParams {
            float itemPitch            = 72.0f;    //!< 選択肢1つぶんの縦の送り
            float itemStepX            = 26.0f;    //!< 選択肢1つぶんの横のずらし（下の行ほど右へ。列全体の中心はcenterXのまま）
            float highlightHeight      = 60.0f;    //!< 強調帯の高さ
            float highlightRollDegrees = -4.0f;    //!< 強調帯の傾き（度。負で右上がり）
            float shadowOffsetX        = 10.0f;    //!< 影の帯のずらしX（強調帯から右下へ）
            float shadowOffsetY        = 8.0f;     //!< 同じくY

            float selectedTextScale   = 1.3f;     //!< 選択中の文字の大きさ（Largeに掛ける倍率）
            float unselectedTextScale = 0.95f;    //!< 非選択の文字の大きさ

            float promptGapX        = 80.0f;     //!< 強調帯の右端からプロンプトの列までの距離
            float promptUpOffsetY   = -40.0f;    //!< 選択中の選択肢から見た[W]のY
            float promptDownOffsetY = 40.0f;     //!< 同じく[S]のY
            float promptConfirmGapX = 120.0f;    //!< [W][S]の列から[F]までの横の距離
            float promptScale       = 1.0f;

            hlslpp::float4 highlightColor    = hlslpp::float4(0.78f, 0.06f, 0.10f, 1.0f);    //!< 強調帯（差し色の赤）
            hlslpp::float4 shadowColor       = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.85f);       //!< 影の帯
            hlslpp::float4 selectedTextColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 itemTextColor     = hlslpp::float4(0.85f, 0.85f, 0.88f, 0.6f);     //!< 非選択は沈ませる
        };

        template <class Archive>
        void load(Archive& archive, GameMenuParams& params) {
            LoadField(archive, "itemPitch", params.itemPitch);
            LoadField(archive, "itemStepX", params.itemStepX);
            LoadField(archive, "highlightHeight", params.highlightHeight);
            LoadField(archive, "highlightRollDegrees", params.highlightRollDegrees);
            LoadField(archive, "shadowOffsetX", params.shadowOffsetX);
            LoadField(archive, "shadowOffsetY", params.shadowOffsetY);
            LoadField(archive, "selectedTextScale", params.selectedTextScale);
            LoadField(archive, "unselectedTextScale", params.unselectedTextScale);
            LoadField(archive, "promptGapX", params.promptGapX);
            LoadField(archive, "promptUpOffsetY", params.promptUpOffsetY);
            LoadField(archive, "promptDownOffsetY", params.promptDownOffsetY);
            LoadField(archive, "promptConfirmGapX", params.promptConfirmGapX);
            LoadField(archive, "promptScale", params.promptScale);
            LoadField(archive, "highlightColor", params.highlightColor);
            LoadField(archive, "shadowColor", params.shadowColor);
            LoadField(archive, "selectedTextColor", params.selectedTextColor);
            LoadField(archive, "itemTextColor", params.itemTextColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const GameMenuParams& GetParams() {
            return GetSystemParams<GameMenuParams>("GameMenu");
        }

        //-------------------------------------------------------------
        //! @brief  i番目の選択肢の中心X（下の行ほど右へずらし、列全体の中心をcenterXに合わせる）
        //-------------------------------------------------------------
        [[nodiscard]]
        float CalculateItemX(float centerX, int index, int count) {
            return centerX + (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * GetParams().itemStepX;
        }

        //! 帯・文字・プロンプトが基準から使う層のずらし量
        constexpr int kShadowLayer    = 0;
        constexpr int kHighlightLayer = 5;
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

        widget.shadowEntity    = CreateUiSlashEntity(registry, context, sortOrderBase + kShadowLayer);
        widget.highlightEntity = CreateUiSlashEntity(registry, context, sortOrderBase + kHighlightLayer);

        // 影は強調帯の動き（出現・行の移動・決定の弾み）にそのまま付いて回る
        UiFollow(registry, widget.shadowEntity, widget.highlightEntity);

        for(Tsukino::ECS::Entity& item : widget.itemEntities)
            item = CreateUiTextEntity(registry, sortOrderBase + kItemLayer, UiTextAlign::Center, UiTextFont::Heading);

        for(Tsukino::ECS::Entity& hit : widget.hitEntities)
            hit = CreatePointerHitRect(registry, context, sortOrderBase + kHitLayer);

        widget.upPrompt      = CreateKeyPrompt(registry, context, L"W", PromptChevron::Up, sortOrderBase + kPromptLayer);
        widget.downPrompt    = CreateKeyPrompt(registry, context, L"S", PromptChevron::Down, sortOrderBase + kPromptLayer);
        widget.confirmPrompt = CreateKeyPrompt(registry, context, L"F", PromptChevron::Right, sortOrderBase + kPromptLayer);

        // [W][S][F]は強調帯の横に付いて回るので、帯の動き（出現・行の移動）に追従させる
        UiFollowPrompt(registry, widget.upPrompt, widget.highlightEntity);
        UiFollowPrompt(registry, widget.downPrompt, widget.highlightEntity);
        UiFollowPrompt(registry, widget.confirmPrompt, widget.highlightEntity);

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

        const GameMenuParams& params = GetParams();

        for(int i = 0; i < kGameMenuMaxItems; ++i) {
            if(i >= count) {
                HideUiText(registry, widget.itemEntities[i]);
                HideUiSprite(registry, widget.hitEntities[i]);
                continue;
            }

            const bool  selected = (i == cursor);
            const float itemX    = CalculateItemX(centerX, i, count);
            const float itemY    = topY + params.itemPitch * static_cast<float>(i);
            const float scale    = GetUiTextScale(UiTextSize::Large) * (selected ? params.selectedTextScale : params.unselectedTextScale);
            PlaceUiText(registry, widget.itemEntities[i], itemX, itemY, scale, labels[i],
                        selected ? params.selectedTextColor : params.itemTextColor);

            // マウスの当たり判定は強調帯の幅×1段の送り。上下の行と隙間なく接するので、行の間で途切れない
            StretchSprite(registry, context, widget.hitEntities[i], itemX, itemY, highlightWidth, params.itemPitch, kHitRectColor);
        }

        //-------------------------------------------------------------
        // 強調帯は少し傾けた斜めの板にし、その右下へ黒い影の帯をずらして重ねる
        //-------------------------------------------------------------
        const float cursorX = CalculateItemX(centerX, cursor, count);
        const float cursorY = topY + params.itemPitch * static_cast<float>(cursor);
        const float roll    = params.highlightRollDegrees * std::numbers::pi_v<float> / 180.0f;
        StretchSpriteRotated(registry, context, widget.highlightEntity, cursorX, cursorY, highlightWidth, params.highlightHeight, roll,
                             params.highlightColor);
        StretchSpriteRotated(registry, context, widget.shadowEntity, cursorX + params.shadowOffsetX, cursorY + params.shadowOffsetY,
                             highlightWidth, params.highlightHeight, roll, params.shadowColor);

        //-------------------------------------------------------------
        // [W][S] は選択中の選択肢の右に縦に並べ、[F] はさらにその右に置く。
        // 選択肢が1つしか無いときは動かす先が無いので [W][S] を出さない
        //-------------------------------------------------------------
        InputPromptStyle style;
        style.scale = params.promptScale;

        const float promptX = cursorX + highlightWidth * 0.5f + params.promptGapX;
        if(count > 1) {
            ShowInputPromptAtScreen(registry, context, widget.upPrompt, promptX, cursorY + params.promptUpOffsetY, style);
            ShowInputPromptAtScreen(registry, context, widget.downPrompt, promptX, cursorY + params.promptDownOffsetY, style);
        } else {
            HideInputPrompt(registry, widget.upPrompt);
            HideInputPrompt(registry, widget.downPrompt);
        }
        if(showConfirmPrompt)
            ShowInputPromptAtScreen(registry, context, widget.confirmPrompt, promptX + params.promptConfirmGapX, cursorY, style);
        else
            HideInputPrompt(registry, widget.confirmPrompt);

        //-------------------------------------------------------------
        // 動き。メニューは時間が止まっている間に開くので、どれも実時間で進める
        //-------------------------------------------------------------
        if(!widget.shown) {
            // 開いた瞬間：選択肢を上から順に横から滑り込ませ、強調帯は選択中の行と同じ順番で出す
            for(int i = 0; i < count; ++i)
                UiAppearFromSide(registry, widget.itemEntities[i], i, true);
            UiAppearFromSide(registry, widget.highlightEntity, cursor, true);
        } else if(cursor != widget.lastCursor) {
            // カーソルが動いた：帯（と付いて回る影・[W][S][F]）を前の行から斜めに滑らせ、選んだ行の文字を軽く弾ませる
            UiSlideFrom(registry, widget.highlightEntity, widget.lastCursorX - cursorX, widget.lastCursorY - cursorY, true);
            UiPunch(registry, widget.itemEntities[cursor], 0.5f, true);
        }

        widget.shown       = true;
        widget.lastCursor  = cursor;
        widget.lastCursorX = cursorX;
        widget.lastCursorY = cursorY;
    }

    //-------------------------------------------------------------
    //! @brief 選択中の選択肢を弾ませる
    //-------------------------------------------------------------
    void PunchGameMenuSelection(Tsukino::ECS::Registry& registry, const GameMenuWidget& widget) {
        if(!widget.shown || widget.lastCursor < 0 || widget.lastCursor >= kGameMenuMaxItems)
            return;
        UiPunch(registry, widget.highlightEntity, 1.0f, true);
        UiPunch(registry, widget.itemEntities[static_cast<size_t>(widget.lastCursor)], 1.0f, true);
    }

    //-------------------------------------------------------------
    //! @brief メニューを丸ごと非表示にする
    //-------------------------------------------------------------
    void HideGameMenu(Tsukino::ECS::Registry& registry, const GameMenuWidget& widget) {
        HideUiSprite(registry, widget.highlightEntity);
        HideUiSprite(registry, widget.shadowEntity);

        for(Tsukino::ECS::Entity item : widget.itemEntities)
            HideUiText(registry, item);

        // 隠したメニューがマウスに反応しないよう、当たり判定も面積ゼロにする
        for(Tsukino::ECS::Entity hit : widget.hitEntities)
            HideUiSprite(registry, hit);

        HideInputPrompt(registry, widget.upPrompt);
        HideInputPrompt(registry, widget.downPrompt);
        HideInputPrompt(registry, widget.confirmPrompt);

        widget.shown      = false;    // 次に出すときは、また滑り込ませる
        widget.lastCursor = -1;
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
