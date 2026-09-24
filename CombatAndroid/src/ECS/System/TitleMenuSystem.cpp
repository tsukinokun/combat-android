//-------------------------------------------------------------
//! @file   TitleMenuSystem.cpp
//! @brief  TitleMenuSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/TitleMenuSystem.hpp>
#include <CombatAndroid/ECS/Component/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/TitleMenuComponent.hpp>
#include <CombatAndroid/ECS/Event/SoundEvent.hpp>
#include <CombatAndroid/ECS/Utility/RunRecord.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/Scene/LoadingScene.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @enum   TitleMenuItem
        //! @brief  タイトルのメニュー項目（並び順どおり）
        //-------------------------------------------------------------
        enum class TitleMenuItem : int {
            Start = 0,    //!< 戦闘を始める
            Controls,     //!< 操作説明を開く
            Options,      //!< オプション画面を開く
            Quit,         //!< ゲームを終了する
            Count,
        };

        //! 項目の文字。TitleMenuItemの並びと揃えること
        const std::array<std::wstring, static_cast<size_t>(TitleMenuItem::Count)> kMenuLabels = {
            L"はじめる",
            L"操作説明",
            L"オプション",
            L"終了",
        };

        //! 操作説明の「もどる」
        const std::array<std::wstring, 1> kControlsMenuLabels = {L"もどる"};

        //-------------------------------------------------------------
        // 操作説明の中身。PlayerSystem・PickupSystem・PauseMenuSystemの実際の割り当てと揃えること
        //-------------------------------------------------------------
        const std::array<std::wstring, kTitleControlsLineCount> kControlActions = {
            L"移動", L"ダッシュ", L"攻撃", L"溜め攻撃", L"回避", L"武器を拾う", L"武器の切り替え", L"カメラ", L"ポーズ",
        };
        const std::array<std::wstring, kTitleControlsLineCount> kControlKeys = {
            L"W A S D",
            L"Shift",
            L"左クリック（続けて押すと連撃）",
            L"左クリック長押し（バトルアックス）",
            L"Space",
            L"F",
            L"マウスホイール",
            L"マウス",
            L"Esc",
        };

        //-------------------------------------------------------------
        // レイアウト（画面中心からのピクセル）。
        // タイトル・メニューは画面の左寄りの1列にまとめ、右半分は3Dの武器（TitleStageSystem）へ譲る
        //-------------------------------------------------------------
        constexpr float kLeftColumnRatio = 0.26f;    //!< 文字の列の中心（画面幅に対する割合）

        //! メニューの強調帯の幅。既定（440）より細くして、帯の右に出るキーの案内を左半分へ収める
        constexpr float kMenuHighlightWidth = 360.0f;

        //! 左側を暗くする板の幅（画面幅に対する割合）と、そこから背景へぼかす帯の幅
        constexpr float kBackdropWidthRatio = 0.42f;
        constexpr float kBackdropFadeRatio  = 0.30f;

        constexpr float kTitleOffsetY    = -250.0f;
        constexpr float kMenuTopOffsetY  = -20.0f;
        constexpr float kBestOffsetY     = 280.0f;

        constexpr float kTitleFontScale    = 3.2f;
        constexpr float kBestFontScale     = 0.85f;

        constexpr float kControlsPanelWidth     = 980.0f;
        constexpr float kControlsPanelHeight    = 720.0f;
        constexpr float kControlsHeaderOffsetY  = -300.0f;
        constexpr float kControlsFirstLineY     = -220.0f;
        constexpr float kControlsLinePitch      = 48.0f;
        constexpr float kControlsActionOffsetX  = -400.0f;    //!< 操作の名前の左端
        constexpr float kControlsKeyOffsetX     = -130.0f;    //!< キーの左端
        constexpr float kControlsMenuOffsetY    = 280.0f;
        constexpr float kControlsHeaderScale    = 1.6f;
        constexpr float kControlsLineScale      = 0.95f;

        //! 文字の下を暗くする色。背景の草原を透かすため不透明にはしない
        const hlslpp::float4 kBackdropColor = hlslpp::float4(0.02f, 0.03f, 0.05f, 0.72f);

        //! 操作説明・オプションを開いている間、画面全体を覆う色（板と文字が背景と混ざらないよう濃くする）
        const hlslpp::float4 kFullBackdropColor = hlslpp::float4(0.02f, 0.03f, 0.05f, 0.92f);
        const hlslpp::float4 kTitleColor         = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        const hlslpp::float4 kBestColor          = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);
        const hlslpp::float4 kControlsPanelColor = hlslpp::float4(0.09f, 0.10f, 0.15f, 1.0f);
        const hlslpp::float4 kControlsActionColor = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);
        const hlslpp::float4 kControlsKeyColor   = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);

        //-------------------------------------------------------------
        //! @brief  ベスト記録の1行を作る
        //! @return 例：「ベスト　生存 9:05　撃破 312　Lv 18　クリア 2回」。記録が無ければその旨
        //-------------------------------------------------------------
        [[nodiscard]]
        std::wstring FormatBestRecord() {
            const RunRecord record = LoadRunRecord();
            if(record.bestSurvivalSeconds <= 0.0f && record.clearCount <= 0)
                return L"まだ記録がありません";

            const int    totalSeconds = static_cast<int>(record.bestSurvivalSeconds);
            std::wstring secondsText  = std::to_wstring(totalSeconds % 60);
            if(secondsText.size() < 2)
                secondsText.insert(0, L"0");

            return L"ベスト　生存 " + std::to_wstring(totalSeconds / 60) + L":" + secondsText + L"　撃破 "
                   + std::to_wstring(record.bestKills) + L"　Lv " + std::to_wstring(record.bestLevel) + L"　クリア "
                   + std::to_wstring(record.clearCount) + L"回";
        }

        //-------------------------------------------------------------
        //! @brief  今の状態に合わせて画面を組み直す
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  title    [in] タイトル画面の状態
        //-------------------------------------------------------------
        void RefreshUi(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const TitleMenuComponent& title) {
            const float screenWidth   = context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f;
            const float screenHeight  = context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;
            const float columnX       = screenWidth * kLeftColumnRatio;

            //-------------------------------------------------------------
            // 背景の板。普段は文字のある左側だけを暗くし、右端は段々に薄くして
            // 3Dの背景へなじませる。操作説明・オプションの間は画面全体を覆う
            //-------------------------------------------------------------
            const bool coverWholeScreen = title.options.isOpen || title.showingControls;

            if(coverWholeScreen) {
                StretchSprite(registry, context, title.backdropEntity, screenCenterX, screenCenterY, screenWidth, screenHeight,
                              kFullBackdropColor);
                for(Tsukino::ECS::Entity fadeEntity : title.backdropFadeEntities)
                    HideUiSprite(registry, fadeEntity);
            } else {
                const float backdropWidth = screenWidth * kBackdropWidthRatio;
                StretchSprite(registry, context, title.backdropEntity, backdropWidth * 0.5f, screenCenterY, backdropWidth, screenHeight,
                              kBackdropColor);

                // 帯を等分し、右へ行くほど薄くする
                const int   fadeCount = static_cast<int>(title.backdropFadeEntities.size());
                const float bandWidth = screenWidth * kBackdropFadeRatio / static_cast<float>(fadeCount);

                for(int i = 0; i < fadeCount; ++i) {
                    const float bandCenterX = backdropWidth + bandWidth * (static_cast<float>(i) + 0.5f);
                    const float alphaScale  = 1.0f - (static_cast<float>(i) + 0.5f) / static_cast<float>(fadeCount);

                    hlslpp::float4 bandColor = kBackdropColor;
                    bandColor.w              = kBackdropColor.w * alphaScale;

                    StretchSprite(registry, context, title.backdropFadeEntities[i], bandCenterX, screenCenterY, bandWidth, screenHeight,
                                  bandColor);
                }
            }

            //-------------------------------------------------------------
            // オプション画面を開いている間は、その板と文字が重ならないようタイトル側を全部隠す
            // （オプション画面は自分で描く）
            //-------------------------------------------------------------
            if(title.options.isOpen) {
                HideUiText(registry, title.titleEntity);
                HideUiText(registry, title.bestEntity);
                HideGameMenu(registry, title.menu);
                return;
            }

            if(!title.showingControls) {
                PlaceUiText(registry, title.titleEntity, columnX, screenCenterY + kTitleOffsetY, kTitleFontScale, L"人造人間0号機", kTitleColor);
                PlaceUiText(registry, title.bestEntity, columnX, screenCenterY + kBestOffsetY, kBestFontScale, FormatBestRecord(), kBestColor);

                // 強調帯を既定（440）より細くして、その右に並ぶキーの案内を武器へ被せない
                ShowGameMenu(registry, context, title.menu, columnX, screenCenterY + kMenuTopOffsetY, kMenuLabels, title.cursorIndex,
                             kMenuHighlightWidth);

                HideUiSprite(registry, title.controlsPanelEntity);
                HideUiText(registry, title.controlsHeaderEntity);
                for(int i = 0; i < kTitleControlsLineCount; ++i) {
                    HideUiText(registry, title.controlsActionEntities[i]);
                    HideUiText(registry, title.controlsKeyEntities[i]);
                }
                HideGameMenu(registry, title.controlsMenu);
                return;
            }

            // 操作説明の板と文字が重なって読みにくくならないよう、タイトル側の文字は隠す
            HideUiText(registry, title.titleEntity);
            HideUiText(registry, title.bestEntity);
            HideGameMenu(registry, title.menu);

            StretchSprite(registry, context, title.controlsPanelEntity, screenCenterX, screenCenterY, kControlsPanelWidth, kControlsPanelHeight,
                          kControlsPanelColor);
            PlaceUiText(registry, title.controlsHeaderEntity, screenCenterX, screenCenterY + kControlsHeaderOffsetY, kControlsHeaderScale,
                        L"操作説明", kTitleColor);

            for(int i = 0; i < kTitleControlsLineCount; ++i) {
                const float lineY = screenCenterY + kControlsFirstLineY + kControlsLinePitch * static_cast<float>(i);
                PlaceUiText(registry, title.controlsActionEntities[i], screenCenterX + kControlsActionOffsetX, lineY, kControlsLineScale,
                            kControlActions[i], kControlsActionColor);
                PlaceUiText(registry, title.controlsKeyEntities[i], screenCenterX + kControlsKeyOffsetX, lineY, kControlsLineScale,
                            kControlKeys[i], kControlsKeyColor);
            }

            ShowGameMenu(registry, context, title.controlsMenu, screenCenterX, screenCenterY + kControlsMenuOffsetY, kControlsMenuLabels, 0);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TitleMenuSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        auto view = registry.View<TitleMenuComponent>();
        for(entt::entity entity : view) {
            TitleMenuComponent& title = view.get<TitleMenuComponent>(entity);

            // 状態を切り替えた（または画面を作った）最初のフレームは組み直すだけにして、入力を拾わない
            if(title.changedThisFrame) {
                title.changedThisFrame = false;
                RefreshUi(registry, *ctx, title);
                continue;
            }

            if(!ctx->inputSystem)
                continue;

            const Tsukino::Input::InputSystem& input = *ctx->inputSystem;

            //-------------------------------------------------------------
            // オプション画面：入力は全てそちらへ回し、閉じたらタイトルを組み直す
            //-------------------------------------------------------------
            if(title.options.isOpen) {
                if(UpdateOptionsMenu(registry, *ctx, title.options))
                    title.changedThisFrame = true;
                continue;
            }

            //-------------------------------------------------------------
            // 操作説明：決定かEscでタイトルのメニューへ戻る
            //-------------------------------------------------------------
            if(title.showingControls) {
                if(IsGameMenuConfirmPressed(input) || input.IsKeyPressed(Tsukino::Input::KeyCode::Escape)) {
                    PlaySound(registry, SoundId::MenuConfirm);
                    title.showingControls  = false;
                    title.changedThisFrame = true;
                }
                continue;
            }

            const int step = ReadGameMenuStep(input);
            if(step != 0) {
                const int nextIndex = std::clamp(title.cursorIndex + step, 0, static_cast<int>(TitleMenuItem::Count) - 1);
                if(nextIndex != title.cursorIndex) {
                    title.cursorIndex = nextIndex;
                    PlaySound(registry, SoundId::MenuMove);
                    RefreshUi(registry, *ctx, title);
                }
            }

            if(!IsGameMenuConfirmPressed(input))
                continue;

            PlaySound(registry, SoundId::MenuConfirm);

            switch(static_cast<TitleMenuItem>(title.cursorIndex)) {
            case TitleMenuItem::Start:
                // ChangeScene()は次のシーンを予約するだけで、実際の切り替えは次フレーム頭で行われる。
                // 戦闘で使うアセットはロード画面が裏スレッドで読み、読み終えてから戦闘シーンへ移る
                // （直接切り替えると、戦闘シーンの初期化で読み込む間ずっと画面が止まる）。
                // タイトルから始めたときだけ操作の案内を出す（リトライからは出さない）
                if(ctx->gameSceneManager)
                    ctx->gameSceneManager->ChangeScene(std::make_unique<CombatAndroid::LoadingScene>(true));
                break;

            case TitleMenuItem::Controls:
                title.showingControls  = true;
                title.changedThisFrame = true;
                break;

            case TitleMenuItem::Options:
                // 開く前にタイトル側を隠す（RefreshUiはoptions.isOpenを見て隠す側へ回る）
                OpenOptionsMenu(registry, *ctx, title.options);
                RefreshUi(registry, *ctx, title);
                break;

            case TitleMenuItem::Quit:
            default:
                // エンジンにアプリ終了のAPIは無く、メインループはProcessMessagesがWM_QUITを
                // 受け取ると抜ける作りなので、それを送る
                ::PostQuitMessage(0);
                break;
            }
        }
    }
}    // namespace CombatAndroid::ECS
