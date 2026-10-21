//-------------------------------------------------------------
//! @file   PauseMenuSystem.cpp
//! @brief  PauseMenuSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Menu/PauseMenuSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/RunResultSystem.hpp>
#include <CombatAndroid/ECS/System/Progression/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Component/Menu/PauseMenuComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Event/Audio/SoundEvent.hpp>
#include <CombatAndroid/ECS/Utility/Input/GameInput.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/UI/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>
#include <CombatAndroid/Scene/TitleScene.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @enum   PauseMenuItem
        //! @brief  ポーズメニューの項目（並び順どおり）
        //-------------------------------------------------------------
        enum class PauseMenuItem : int {
            Resume = 0,    //!< 再開
            Options,       //!< オプション画面を開く
            Retry,         //!< 最初からやり直す
            Title,         //!< タイトルへ戻る
            Count,
        };

        //! 項目の文字。PauseMenuItemの並びと揃えること
        const std::array<std::wstring, static_cast<size_t>(PauseMenuItem::Count)> kMenuLabels = {
            L"再開",
            L"オプション",
            L"リトライ",
            L"タイトルへ",
        };

        //-------------------------------------------------------------
        //! @struct PauseMenuParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/PauseMenu.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct PauseMenuParams {
            float columnXRatio = 0.3f;         //!< 見出しとメニューの列の中心X（画面幅に対する割合）。中央を外して左へ寄せる
            float titleOffsetY = -170.0f;    //!< 画面中心から見た「PAUSE」のY
            float menuTopOffsetY = -40.0f;     //!< 画面中心から見た1つ目の項目のY

            //! 見出しの後ろに敷く斜めの帯。画面の左端の外から伸ばし、見出しの右 bandTailX の所で斜めに切る
            float bandTailX = 260.0f;
            float bandHeight = 110.0f;
            float bandRollDegrees = -5.0f;

            hlslpp::float4 backdropColor = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.6f);
            hlslpp::float4 titleColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 bandColor = hlslpp::float4(0.78f, 0.06f, 0.1f, 1.0f);
        };

        template <class Archive>
        void load(Archive& archive, PauseMenuParams& params) {
            LoadField(archive, "columnXRatio", params.columnXRatio);
            LoadField(archive, "titleOffsetY", params.titleOffsetY);
            LoadField(archive, "menuTopOffsetY", params.menuTopOffsetY);
            LoadField(archive, "bandTailX", params.bandTailX);
            LoadField(archive, "bandHeight", params.bandHeight);
            LoadField(archive, "bandRollDegrees", params.bandRollDegrees);
            LoadField(archive, "backdropColor", params.backdropColor);
            LoadField(archive, "titleColor", params.titleColor);
            LoadField(archive, "bandColor", params.bandColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const PauseMenuParams& GetParams() {
            return GetSystemParams<PauseMenuParams>("PauseMenu");
        }

        //-------------------------------------------------------------
        //! @brief  メニューを表示し直す
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  pause    [in] 対象のポーズメニュー
        //-------------------------------------------------------------
        void RefreshUi(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const PauseMenuComponent& pause) {
            const PauseMenuParams& params = GetParams();

            const float screenWidth   = context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f;
            const float screenHeight  = context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;

            StretchSprite(registry, context, pause.backdropEntity, screenCenterX, screenCenterY, screenWidth, screenHeight, params.backdropColor);

            // オプション画面を開いている間は暗転板だけ残し、見出しとメニューはオプションの板と重ならないよう隠す
            if(pause.options.isOpen) {
                HideUiText(registry, pause.titleEntity);
                HideUiSprite(registry, pause.bandEntity);
                HideGameMenu(registry, pause.menu);
                return;
            }

            // 見出しとメニューは中央を外して左の列に置き、見出しの下には画面の左端から斜めの帯を敷く
            const float columnX = screenWidth * params.columnXRatio;
            const float titleY  = screenCenterY + params.titleOffsetY;
            PlaceUiBandFromLeft(registry, context, pause.bandEntity, columnX + params.bandTailX, titleY, params.bandHeight, params.bandRollDegrees,
                                params.bandColor);
            PlaceUiText(registry, pause.titleEntity, columnX, titleY, GetUiTextScale(UiTextSize::Display), L"PAUSE", params.titleColor);
            UiIdle(registry, pause.titleEntity, 1.0f, true);    // 見出しを止まったままにしない（ポーズ中なので実時間）
            ShowGameMenu(registry, context, pause.menu, columnX, screenCenterY + params.menuTopOffsetY, kMenuLabels, pause.cursorIndex);
        }

        //-------------------------------------------------------------
        //! @brief  メニューを隠す
        //! @param  registry [in] ECSレジストリ
        //! @param  pause    [in] 対象のポーズメニュー
        //-------------------------------------------------------------
        void HideUi(Tsukino::ECS::Registry& registry, const PauseMenuComponent& pause) {
            HideUiSprite(registry, pause.backdropEntity);
            HideUiSprite(registry, pause.bandEntity);
            HideUiText(registry, pause.titleEntity);
            HideGameMenu(registry, pause.menu);
            HideOptionsMenu(registry, pause.options);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 今ポーズで進行を止めているかを問い合わせる
    //-------------------------------------------------------------
    bool IsPauseMenuActive(Tsukino::ECS::Registry& registry) {
        auto view = registry.View<PauseMenuComponent>();
        for(entt::entity entity : view) {
            const PauseMenuComponent& pause = view.get<PauseMenuComponent>(entity);
            return pause.isOpen || pause.closingBlockFrames > 0;
        }
        return false;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PauseMenuSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->inputSystem)
            return;

        const Tsukino::Input::InputSystem& input = *ctx->inputSystem;

        auto view = registry.View<PlayerComponent, PauseMenuComponent>();
        for(entt::entity entity : view) {
            PauseMenuComponent& pause = view.get<PauseMenuComponent>(entity);

            if(!pause.isOpen) {
                //-------------------------------------------------------------
                // 閉じた次のフレーム。決定に使ったFが流れきるまで、この1フレームだけ停止を延長する
                //-------------------------------------------------------------
                if(pause.closingBlockFrames > 0) {
                    --pause.closingBlockFrames;
                    SuppressAllMoveInput(registry);
                    continue;
                }

                //-------------------------------------------------------------
                // Esc・STARTで開く。スキル選択中はメニュー同士が重なるので開かない。
                // 走行が終わった後はリザルトがリトライ／タイトルへの道を持っているので開かない。
                // カットシーン中は演出への操作を渡したくないので開かない
                //-------------------------------------------------------------
                const bool windowFocused = !ctx->window || ctx->window->IsFocused();
                if(!windowFocused || !IsActionPressed(input, GameAction::Pause))
                    continue;
                if(IsSkillSelectActive(registry) || IsRunEnded(registry) || IsCutsceneActive(registry))
                    continue;

                pause.isOpen          = true;
                pause.cursorIndex     = 0;
                pause.openedThisFrame = true;
                PlaySound(registry, SoundId::MenuConfirm);

                // 止めている間にヒットストップが残ると、再開した後もスローが続いてしまう
                ClearAllHitStop(registry);
                SuppressAllMoveInput(registry);
                RefreshUi(registry, *ctx, pause);

                // 開いた瞬間だけ、帯と見出しを横から滑り込ませる（選択肢の動きはShowGameMenuが付ける）
                UiAppear(registry, pause.bandEntity, UiAppearDesc{.slideX = -240.0f, .unscaledTime = true});
                UiAppear(registry, pause.titleEntity,
                         UiAppearDesc{.slideX = -GetUiMotionParams().appearSlide * 2.0f, .duration = GetUiMotionParams().appearDuration * 1.5f, .unscaledTime = true});
                continue;
            }

            // 開いている間はプレイヤーも敵も移動入力を持たない状態に固定する
            SuppressAllMoveInput(registry);

            // 開くのに使ったEscで、そのまま閉じない
            if(pause.openedThisFrame) {
                pause.openedThisFrame = false;
                continue;
            }

            //-------------------------------------------------------------
            // オプション画面：入力は全てそちらへ回す。オプションを閉じたEscで
            // ポーズまで閉じないよう、閉じたフレームはここで終える
            //-------------------------------------------------------------
            if(pause.options.isOpen) {
                if(UpdateOptionsMenu(registry, *ctx, pause.options))
                    RefreshUi(registry, *ctx, pause);
                continue;
            }

            //-------------------------------------------------------------
            // カーソル移動と決定。Escをもう一度押しても再開する
            //-------------------------------------------------------------
            const int             step    = ReadGameMenuStep(input);
            const GameMenuPointer pointer = ReadGameMenuPointer(registry, input, pause.menu);
            if(step != 0 || pointer.hoverIndex >= 0) {
                // マウスが乗った選択肢があればそちらへ合わせる
                const int nextIndex = pointer.hoverIndex >= 0 ? pointer.hoverIndex
                                                              : std::clamp(pause.cursorIndex + step, 0, static_cast<int>(PauseMenuItem::Count) - 1);
                if(nextIndex != pause.cursorIndex) {
                    pause.cursorIndex = nextIndex;
                    PlaySound(registry, SoundId::MenuMove);
                    RefreshUi(registry, *ctx, pause);
                }
            }

            // 暗転が始まったら、もう選択は受け付けない（連打で二重に切り替えないため）
            if(IsScreenFadingOut(registry))
                continue;

            // 戻る（Esc・B）か、開いたのと同じボタン（START）で閉じる
            const bool resumeByEscape = IsGameMenuCancelPressed(input) || IsActionPressed(input, GameAction::Pause);
            if(!resumeByEscape && !IsGameMenuConfirmPressed(input) && !pointer.clicked)
                continue;

            const PauseMenuItem selected = resumeByEscape ? PauseMenuItem::Resume : static_cast<PauseMenuItem>(pause.cursorIndex);
            PlaySound(registry, SoundId::MenuConfirm);
            PunchGameMenuSelection(registry, pause.menu);

            switch(selected) {
            case PauseMenuItem::Retry:
                // 黒く覆ってから切り替える。実際のChangeSceneは暗転しきった時点で
                // ScreenFadeSystemが呼ぶ（リトライでは操作の案内を出さない）
                RequestSceneChangeWithFade(registry, []() { return std::make_unique<CombatAndroid::CombatAndroidScene>(); });
                break;

            case PauseMenuItem::Title:
                RequestSceneChangeWithFade(registry, []() { return std::make_unique<CombatAndroid::TitleScene>(); });
                break;

            case PauseMenuItem::Options:
                OpenOptionsMenu(registry, *ctx, pause.options);
                RefreshUi(registry, *ctx, pause);
                break;

            case PauseMenuItem::Resume:
            default:
                pause.isOpen             = false;
                pause.closingBlockFrames = 1;
                HideUi(registry, pause);
                break;
            }
        }
    }
}    // namespace CombatAndroid::ECS
