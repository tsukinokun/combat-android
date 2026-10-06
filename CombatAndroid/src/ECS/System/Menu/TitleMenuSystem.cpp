//-------------------------------------------------------------
//! @file   TitleMenuSystem.cpp
//! @brief  TitleMenuSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Menu/TitleMenuSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Progression/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/Menu/TitleMenuComponent.hpp>
#include <CombatAndroid/ECS/Event/Audio/SoundEvent.hpp>
#include <CombatAndroid/ECS/Utility/Save/RunRecord.hpp>
#include <CombatAndroid/ECS/Utility/UI/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Component/Menu/TitleStageComponent.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>

#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
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
            L"近づくだけ",
            L"マウスホイール",
            L"マウス",
            L"Esc",
        };

        //-------------------------------------------------------------
        //! @struct TitleMenuParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/TitleMenu.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct TitleMenuParams {
            //-------------------------------------------------------------
            // レイアウト（画面中心からのピクセル）。
            // タイトル・メニューは画面の左寄りの1列にまとめ、右半分は3Dの武器（TitleStageSystem）へ譲る
            //-------------------------------------------------------------
            float leftColumnRatio = 0.26f;    //!< 文字の列の中心（画面幅に対する割合）

            //! メニューの強調帯の幅。既定（440）より細くして、帯の右に出るキーの案内を左半分へ収める
            float menuHighlightWidth = 360.0f;

            //! 左側を暗くする板の幅（画面幅に対する割合）と、そこから背景へぼかす帯の幅
            float backdropWidthRatio = 0.42f;
            float backdropFadeRatio = 0.30f;

            float titleOffsetY = -250.0f;

            //! タイトルの幅の上限（画面幅に対する割合）。超えるときは文字全体を縮める。
            //! 列の中心（leftColumnRatio）から左右に半分ずつ取っても、暗い板（backdropWidthRatio）の内側に収まる値にする
            float titleMaxWidthRatio = 0.30f;
            float menuTopOffsetY = -20.0f;
            float bestOffsetY = 280.0f;


            float controlsPanelWidth = 980.0f;
            float controlsPanelHeight = 720.0f;
            float controlsHeaderOffsetY = -300.0f;
            float controlsFirstLineY = -220.0f;
            float controlsLinePitch = 48.0f;
            float controlsActionOffsetX = -360.0f;    //!< 操作の名前の左端
            float controlsKeyOffsetX = -90.0f;    //!< キーの左端
            float controlsMenuOffsetY = 280.0f;

            //! 文字の下を暗くする色。背景の草原を透かすため不透明にはしない
            hlslpp::float4 backdropColor = hlslpp::float4(0.02f, 0.03f, 0.05f, 0.72f);

            //! 操作説明・オプションを開いている間、画面全体を覆う色（板と文字が背景と混ざらないよう濃くする）
            hlslpp::float4 fullBackdropColor = hlslpp::float4(0.02f, 0.03f, 0.05f, 0.92f);
            hlslpp::float4 titleColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 bestColor = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);
            hlslpp::float4 controlsPanelColor = hlslpp::float4(0.09f, 0.10f, 0.15f, 1.0f);
            hlslpp::float4 controlsActionColor = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);
            hlslpp::float4 controlsKeyColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        template <class Archive>
        void load(Archive& archive, TitleMenuParams& params) {
            LoadField(archive, "leftColumnRatio", params.leftColumnRatio);
            LoadField(archive, "menuHighlightWidth", params.menuHighlightWidth);
            LoadField(archive, "backdropWidthRatio", params.backdropWidthRatio);
            LoadField(archive, "backdropFadeRatio", params.backdropFadeRatio);
            LoadField(archive, "titleOffsetY", params.titleOffsetY);
            LoadField(archive, "titleMaxWidthRatio", params.titleMaxWidthRatio);
            LoadField(archive, "menuTopOffsetY", params.menuTopOffsetY);
            LoadField(archive, "bestOffsetY", params.bestOffsetY);
            LoadField(archive, "controlsPanelWidth", params.controlsPanelWidth);
            LoadField(archive, "controlsPanelHeight", params.controlsPanelHeight);
            LoadField(archive, "controlsHeaderOffsetY", params.controlsHeaderOffsetY);
            LoadField(archive, "controlsFirstLineY", params.controlsFirstLineY);
            LoadField(archive, "controlsLinePitch", params.controlsLinePitch);
            LoadField(archive, "controlsActionOffsetX", params.controlsActionOffsetX);
            LoadField(archive, "controlsKeyOffsetX", params.controlsKeyOffsetX);
            LoadField(archive, "controlsMenuOffsetY", params.controlsMenuOffsetY);
            LoadField(archive, "backdropColor", params.backdropColor);
            LoadField(archive, "fullBackdropColor", params.fullBackdropColor);
            LoadField(archive, "titleColor", params.titleColor);
            LoadField(archive, "bestColor", params.bestColor);
            LoadField(archive, "controlsPanelColor", params.controlsPanelColor);
            LoadField(archive, "controlsActionColor", params.controlsActionColor);
            LoadField(archive, "controlsKeyColor", params.controlsKeyColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const TitleMenuParams& GetParams() {
            static const TitleMenuParams s_params = LoadSystemParams<TitleMenuParams>("TitleMenu");
            return s_params;
        }

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
        //! @brief  「はじめる」の見せ場が始まっているか
        //! @param  registry [in] ECSレジストリ
        //! @return 始まっていればtrue
        //-------------------------------------------------------------
        [[nodiscard]]
        bool IsTitleLaunching(Tsukino::ECS::Registry& registry) {
            bool launching = false;

            auto view = registry.View<TitleStageComponent>();
            view.each([&](entt::entity, const TitleStageComponent& stage) { launching = launching || stage.launchRequested; });

            return launching;
        }

        //-------------------------------------------------------------
        //! @brief  「はじめる」の見せ場を始めるよう頼む
        //! @param  registry [in,out] ECSレジストリ
        //! @note   進行と、ロード画面への切り替えはTitleStageSystemが行う
        //-------------------------------------------------------------
        void RequestTitleLaunch(Tsukino::ECS::Registry& registry) {
            auto view = registry.View<TitleStageComponent>();
            view.each([](entt::entity, TitleStageComponent& stage) { stage.launchRequested = true; });
        }

        //-------------------------------------------------------------
        //! @brief  今の状態に合わせて画面を組み直す
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  title    [in] タイトル画面の状態
        //-------------------------------------------------------------
        void RefreshUi(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const TitleMenuComponent& title) {
            const TitleMenuParams& params = GetParams();

            const float screenWidth   = context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f;
            const float screenHeight  = context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;
            const float columnX       = screenWidth * params.leftColumnRatio;

            //-------------------------------------------------------------
            // 背景の板。普段は文字のある左側だけを暗くし、右端は段々に薄くして
            // 3Dの背景へなじませる。操作説明・オプションの間は画面全体を覆う
            //-------------------------------------------------------------
            const bool coverWholeScreen = title.options.isOpen || title.showingControls;

            if(coverWholeScreen) {
                StretchSprite(registry, context, title.backdropEntity, screenCenterX, screenCenterY, screenWidth, screenHeight,
                              params.fullBackdropColor);
                for(Tsukino::ECS::Entity fadeEntity : title.backdropFadeEntities)
                    HideUiSprite(registry, fadeEntity);
            } else {
                const float backdropWidth = screenWidth * params.backdropWidthRatio;
                StretchSprite(registry, context, title.backdropEntity, backdropWidth * 0.5f, screenCenterY, backdropWidth, screenHeight,
                              params.backdropColor);

                // 帯を等分し、右へ行くほど薄くする
                const int   fadeCount = static_cast<int>(title.backdropFadeEntities.size());
                const float bandWidth = screenWidth * params.backdropFadeRatio / static_cast<float>(fadeCount);

                for(int i = 0; i < fadeCount; ++i) {
                    const float bandCenterX = backdropWidth + bandWidth * (static_cast<float>(i) + 0.5f);
                    const float alphaScale  = 1.0f - (static_cast<float>(i) + 0.5f) / static_cast<float>(fadeCount);

                    hlslpp::float4 bandColor = params.backdropColor;
                    bandColor.w              = params.backdropColor.w * alphaScale;

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
                PlaceUiText(registry, title.titleEntity, columnX, screenCenterY + params.titleOffsetY, GetUiTextScale(UiTextSize::Display), L"人造人間0号機", params.titleColor);
                if(auto* titleFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(title.titleEntity))
                    titleFont->maxWidth = screenWidth * params.titleMaxWidthRatio;
                PlaceUiText(registry, title.bestEntity, columnX, screenCenterY + params.bestOffsetY, GetUiTextScale(UiTextSize::Small), FormatBestRecord(), params.bestColor);

                // 強調帯を既定（440）より細くして、その右に並ぶキーの案内を武器へ被せない
                ShowGameMenu(registry, context, title.menu, columnX, screenCenterY + params.menuTopOffsetY, kMenuLabels, title.cursorIndex,
                             params.menuHighlightWidth);

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

            StretchSprite(registry, context, title.controlsPanelEntity, screenCenterX, screenCenterY, params.controlsPanelWidth, params.controlsPanelHeight,
                          params.controlsPanelColor);
            PlaceUiText(registry, title.controlsHeaderEntity, screenCenterX, screenCenterY + params.controlsHeaderOffsetY, GetUiTextScale(UiTextSize::Heading),
                        L"操作説明", params.titleColor);

            for(int i = 0; i < kTitleControlsLineCount; ++i) {
                const float lineY = screenCenterY + params.controlsFirstLineY + params.controlsLinePitch * static_cast<float>(i);
                PlaceUiText(registry, title.controlsActionEntities[i], screenCenterX + params.controlsActionOffsetX, lineY, GetUiTextScale(UiTextSize::Body),
                            kControlActions[i], params.controlsActionColor);
                PlaceUiText(registry, title.controlsKeyEntities[i], screenCenterX + params.controlsKeyOffsetX, lineY, GetUiTextScale(UiTextSize::Body),
                            kControlKeys[i], params.controlsKeyColor);
            }

            ShowGameMenu(registry, context, title.controlsMenu, screenCenterX, screenCenterY + params.controlsMenuOffsetY, kControlsMenuLabels, 0);
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

            // 見せ場や暗転が始まったら、もう入力は拾わない（連打で二重に始めないため）
            if(IsTitleLaunching(registry) || IsScreenFadingOut(registry))
                continue;

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
                const bool clicked = ReadGameMenuPointer(registry, input, title.controlsMenu).clicked;
                if(IsGameMenuConfirmPressed(input) || clicked || input.IsKeyPressed(Tsukino::Input::KeyCode::Escape)) {
                    PlaySound(registry, SoundId::MenuConfirm);
                    title.showingControls  = false;
                    title.changedThisFrame = true;
                }
                continue;
            }

            const int             step    = ReadGameMenuStep(input);
            const GameMenuPointer pointer = ReadGameMenuPointer(registry, input, title.menu);
            if(step != 0 || pointer.hoverIndex >= 0) {
                // マウスが乗った選択肢があればそちらへ合わせる
                const int nextIndex = pointer.hoverIndex >= 0 ? pointer.hoverIndex
                                                              : std::clamp(title.cursorIndex + step, 0, static_cast<int>(TitleMenuItem::Count) - 1);
                if(nextIndex != title.cursorIndex) {
                    title.cursorIndex = nextIndex;
                    PlaySound(registry, SoundId::MenuMove);
                    RefreshUi(registry, *ctx, title);
                }
            }

            if(!IsGameMenuConfirmPressed(input) && !pointer.clicked)
                continue;

            PlaySound(registry, SoundId::MenuConfirm);

            switch(static_cast<TitleMenuItem>(title.cursorIndex)) {
            case TitleMenuItem::Start:
                //-------------------------------------------------------------
                // ここでは場面を切り替えず、武器が飛んでくる見せ場を頼むだけにする。
                // 見せ場が終わった時点でTitleStageSystemがロード画面へ切り替える
                // （戦闘で使うアセットはロード画面が裏スレッドで読む。直接戦闘シーンへ
                // 切り替えると、その初期化で読み込む間ずっと画面が止まる）
                //-------------------------------------------------------------
                RequestTitleLaunch(registry);
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
