//-------------------------------------------------------------
//! @file    OptionsMenu.cpp
//! @brief   オプション画面の部品の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/OptionsMenu.hpp>
#include <CombatAndroid/ECS/Event/SoundEvent.hpp>
#include <CombatAndroid/ECS/Utility/Bgm.hpp>
#include <CombatAndroid/ECS/Utility/GameSettings.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Window.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @enum   OptionsItem
        //! @brief  オプション画面の項目（並び順どおり）
        //-------------------------------------------------------------
        enum class OptionsItem : int {
            MouseSensitivity = 0,    //!< マウス感度
            BgmVolume,               //!< BGMの音量
            SeVolume,                //!< 効果音の音量
            ScreenShake,             //!< 画面揺れ
            Back,                    //!< もどる
            Count,
        };

        static_assert(static_cast<int>(OptionsItem::Count) <= kGameMenuMaxItems, "オプションの項目数はGameMenuに並べられる数以内にすること");

        //-------------------------------------------------------------
        // レイアウト（画面中心からのピクセル）
        //-------------------------------------------------------------
        constexpr float kPanelWidth      = 1000.0f;
        constexpr float kPanelHeight     = 620.0f;
        constexpr float kHeaderOffsetY   = -240.0f;
        constexpr float kMenuTopOffsetY  = -140.0f;
        constexpr float kHintOffsetY     = 250.0f;
        constexpr float kHighlightWidth  = 620.0f;    //!< 「項目名　< 値 >」が収まる幅
        constexpr float kHeaderFontScale = 1.6f;
        constexpr float kHintFontScale   = 0.8f;

        //! [A][D] を強調帯の左に並べる位置（帯の左端からの距離）
        constexpr float kDecreasePromptGapX = 150.0f;
        constexpr float kIncreasePromptGapX = 70.0f;

        //! 描画層（基準からのずらし量）。GameMenuWidgetが+0〜+24を使うので、その奥と手前に置く
        constexpr int kPanelLayer  = 0;
        constexpr int kMenuLayer   = 10;
        constexpr int kTextLayer   = 40;
        constexpr int kPromptLayer = 40;

        const hlslpp::float4 kPanelColor  = hlslpp::float4(0.09f, 0.10f, 0.15f, 0.97f);    //!< タイトルの操作説明の板と同じ紺
        const hlslpp::float4 kHeaderColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        const hlslpp::float4 kHintColor   = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);

        //-------------------------------------------------------------
        //! @brief  項目1行ぶんの文字を作る
        //! @param  item [in] 項目
        //! @return 例：「マウス感度　< ×1.25 >」
        //-------------------------------------------------------------
        [[nodiscard]]
        std::wstring FormatItem(OptionsItem item) {
            const GameSettings& settings = GetGameSettings();

            wchar_t value[32] = {};
            switch(item) {
            case OptionsItem::MouseSensitivity:
                std::swprintf(value, std::size(value), L"×%.2f", settings.mouseSensitivityScale);
                return std::wstring(L"マウス感度　< ") + value + L" >";
            case OptionsItem::BgmVolume:
                return L"BGMの音量　< " + std::to_wstring(settings.bgmVolumePercent) + L"% >";
            case OptionsItem::SeVolume:
                return L"効果音の音量　< " + std::to_wstring(settings.seVolumePercent) + L"% >";
            case OptionsItem::ScreenShake:
                return std::wstring(L"画面揺れ　< ") + (settings.screenShakeEnabled ? L"ON" : L"OFF") + L" >";
            case OptionsItem::Back:
            default:
                return L"もどる";
            }
        }

        //-------------------------------------------------------------
        //! @brief  選択中の項目の値を1段動かす
        //! @param  item [in] 項目
        //! @param  step [in] -1 か +1
        //! @return 値が変わったらtrue（端で止まったときはfalse）
        //-------------------------------------------------------------
        bool StepItemValue(OptionsItem item, int step) {
            GameSettings& settings = GetGameSettings();

            switch(item) {
            case OptionsItem::MouseSensitivity: {
                const float next = std::clamp(settings.mouseSensitivityScale + kMouseSensitivityStep * static_cast<float>(step),
                                              kMouseSensitivityMin, kMouseSensitivityMax);
                if(next == settings.mouseSensitivityScale)
                    return false;
                settings.mouseSensitivityScale = next;
                return true;
            }
            case OptionsItem::BgmVolume: {
                const int next = std::clamp(settings.bgmVolumePercent + kVolumePercentStep * step, 0, 100);
                if(next == settings.bgmVolumePercent)
                    return false;
                settings.bgmVolumePercent = next;
                return true;
            }
            case OptionsItem::SeVolume: {
                const int next = std::clamp(settings.seVolumePercent + kVolumePercentStep * step, 0, 100);
                if(next == settings.seVolumePercent)
                    return false;
                settings.seVolumePercent = next;
                return true;
            }
            case OptionsItem::ScreenShake:
                // ON/OFFの2値なので、左右どちらでも切り替える
                settings.screenShakeEnabled = !settings.screenShakeEnabled;
                return true;
            default:
                return false;
            }
        }

        //-------------------------------------------------------------
        //! @brief  今の状態で画面を組み直す
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  options  [in] 対象
        //-------------------------------------------------------------
        void RefreshUi(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const OptionsMenuState& options) {
            const float screenWidth   = context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f;
            const float screenHeight  = context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;

            StretchSprite(registry, context, options.panelEntity, screenCenterX, screenCenterY, kPanelWidth, kPanelHeight, kPanelColor);
            PlaceUiText(registry, options.headerEntity, screenCenterX, screenCenterY + kHeaderOffsetY, kHeaderFontScale, L"オプション", kHeaderColor);
            PlaceUiText(registry, options.hintEntity, screenCenterX, screenCenterY + kHintOffsetY, kHintFontScale,
                        L"W / S：項目を選ぶ　　A / D：値を変える　　Esc：もどる", kHintColor);

            std::array<std::wstring, static_cast<size_t>(OptionsItem::Count)> labels;
            for(int i = 0; i < static_cast<int>(OptionsItem::Count); ++i)
                labels[static_cast<size_t>(i)] = FormatItem(static_cast<OptionsItem>(i));

            const float menuTopY = screenCenterY + kMenuTopOffsetY;
            ShowGameMenu(registry, context, options.menu, screenCenterX, menuTopY, labels, options.cursorIndex, kHighlightWidth);

            //-------------------------------------------------------------
            // [A][D] は値を持つ項目を選んでいるときだけ、強調帯の左に出す
            //-------------------------------------------------------------
            if(static_cast<OptionsItem>(options.cursorIndex) == OptionsItem::Back) {
                HideInputPrompt(registry, options.decreasePrompt);
                HideInputPrompt(registry, options.increasePrompt);
                return;
            }

            InputPromptStyle style;
            const float      cursorY  = menuTopY + 72.0f * static_cast<float>(options.cursorIndex);    // GameMenuの1段の送り（kItemPitch）と揃える
            const float      bandLeft = screenCenterX - kHighlightWidth * 0.5f;
            ShowInputPromptAtScreen(registry, context, options.decreasePrompt, bandLeft - kDecreasePromptGapX, cursorY, style);
            ShowInputPromptAtScreen(registry, context, options.increasePrompt, bandLeft - kIncreasePromptGapX, cursorY, style);
        }

        //-------------------------------------------------------------
        //! @brief  キーキャップのプロンプトを1つ作る
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
    //! @brief オプション画面のエンティティ一式を非表示で作る
    //-------------------------------------------------------------
    OptionsMenuState CreateOptionsMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                       int sortOrderBase) {
        OptionsMenuState options;
        options.panelEntity    = CreateUiRectEntity(registry, context, sortOrderBase + kPanelLayer);
        options.headerEntity   = CreateUiTextEntity(registry, sortOrderBase + kTextLayer, UiTextAlign::Center);
        options.hintEntity     = CreateUiTextEntity(registry, sortOrderBase + kTextLayer, UiTextAlign::Center);
        options.menu           = CreateGameMenuWidget(registry, context, sortOrderBase + kMenuLayer);
        options.decreasePrompt = CreateKeyPrompt(registry, context, L"A", PromptChevron::Left, sortOrderBase + kPromptLayer);
        options.increasePrompt = CreateKeyPrompt(registry, context, L"D", PromptChevron::Right, sortOrderBase + kPromptLayer);
        return options;
    }

    //-------------------------------------------------------------
    //! @brief オプション画面を開く
    //-------------------------------------------------------------
    void OpenOptionsMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, OptionsMenuState& options) {
        options.isOpen          = true;
        options.cursorIndex     = 0;
        options.openedThisFrame = true;
        options.changed         = false;
        options.bgmVolumeAtOpen = GetGameSettings().bgmVolumePercent;
        RefreshUi(registry, context, options);
    }

    //-------------------------------------------------------------
    //! @brief 開いているオプション画面の入力を処理する
    //-------------------------------------------------------------
    bool UpdateOptionsMenu(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, OptionsMenuState& options) {
        if(!options.isOpen || !context.inputSystem)
            return false;

        // 開くのに使った決定入力を、そのまま拾わない
        if(options.openedThisFrame) {
            options.openedThisFrame = false;
            return false;
        }

        const Tsukino::Input::InputSystem& input = *context.inputSystem;
        const OptionsItem                  item  = static_cast<OptionsItem>(options.cursorIndex);

        //-------------------------------------------------------------
        // 項目の移動
        //-------------------------------------------------------------
        const int step = ReadGameMenuStep(input);
        if(step != 0) {
            const int nextIndex = std::clamp(options.cursorIndex + step, 0, static_cast<int>(OptionsItem::Count) - 1);
            if(nextIndex != options.cursorIndex) {
                options.cursorIndex = nextIndex;
                PlaySound(registry, SoundId::MenuMove);
                RefreshUi(registry, context, options);
            }
            return false;
        }

        //-------------------------------------------------------------
        // 値の変更。画面揺れはFでも切り替えられるようにする（ON/OFFは決定で押したくなるため）。
        // 変えた直後の効果音は新しい音量で鳴るので、効果音の音量はその場で確かめられる
        //-------------------------------------------------------------
        int valueStep = ReadGameMenuValueStep(input);
        if(valueStep == 0 && item == OptionsItem::ScreenShake && IsGameMenuConfirmPressed(input))
            valueStep = 1;

        if(valueStep != 0) {
            if(StepItemValue(item, valueStep)) {
                options.changed = true;
                PlaySound(registry, SoundId::MenuMove);
                RefreshUi(registry, context, options);
            }
            return false;
        }

        //-------------------------------------------------------------
        // 閉じる：「もどる」の決定かEsc。変えていれば保存し、BGMの音量が変わっていれば鳴らし直す
        //-------------------------------------------------------------
        const bool closeByEscape  = input.IsKeyPressed(Tsukino::Input::KeyCode::Escape);
        const bool closeByConfirm = item == OptionsItem::Back && IsGameMenuConfirmPressed(input);
        if(!closeByEscape && !closeByConfirm)
            return false;

        PlaySound(registry, SoundId::MenuConfirm);

        if(options.changed)
            SaveGameSettings();
        if(GetGameSettings().bgmVolumePercent != options.bgmVolumeAtOpen)
            ReapplyBgmVolume(context);

        options.isOpen = false;
        HideOptionsMenu(registry, options);
        return true;
    }

    //-------------------------------------------------------------
    //! @brief オプション画面を隠す
    //-------------------------------------------------------------
    void HideOptionsMenu(Tsukino::ECS::Registry& registry, const OptionsMenuState& options) {
        HideUiSprite(registry, options.panelEntity);
        HideUiText(registry, options.headerEntity);
        HideUiText(registry, options.hintEntity);
        HideGameMenu(registry, options.menu);
        HideInputPrompt(registry, options.decreasePrompt);
        HideInputPrompt(registry, options.increasePrompt);
    }
}    // namespace CombatAndroid::ECS
