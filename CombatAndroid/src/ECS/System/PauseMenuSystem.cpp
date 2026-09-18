//-------------------------------------------------------------
//! @file   PauseMenuSystem.cpp
//! @brief  PauseMenuSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PauseMenuSystem.hpp>
#include <CombatAndroid/ECS/System/RunResultSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Component/PauseMenuComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Event/SoundEvent.hpp>
#include <CombatAndroid/ECS/Utility/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>
#include <CombatAndroid/Scene/TitleScene.hpp>

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
            Retry,         //!< 最初からやり直す
            Title,         //!< タイトルへ戻る
            Count,
        };

        //! 項目の文字。PauseMenuItemの並びと揃えること
        const std::array<std::wstring, static_cast<size_t>(PauseMenuItem::Count)> kMenuLabels = {
            L"再開",
            L"リトライ",
            L"タイトルへ",
        };

        constexpr float kTitleOffsetY   = -170.0f;    //!< 画面中心から見た「PAUSE」のY
        constexpr float kMenuTopOffsetY = -40.0f;     //!< 画面中心から見た1つ目の項目のY
        constexpr float kTitleFontScale = 2.4f;

        const hlslpp::float4 kBackdropColor = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.6f);
        const hlslpp::float4 kTitleColor    = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);

        //-------------------------------------------------------------
        //! @brief  メニューを表示し直す
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  pause    [in] 対象のポーズメニュー
        //-------------------------------------------------------------
        void RefreshUi(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const PauseMenuComponent& pause) {
            const float screenWidth   = context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f;
            const float screenHeight  = context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;

            StretchSprite(registry, context, pause.backdropEntity, screenCenterX, screenCenterY, screenWidth, screenHeight, kBackdropColor);
            PlaceUiText(registry, pause.titleEntity, screenCenterX, screenCenterY + kTitleOffsetY, kTitleFontScale, L"PAUSE", kTitleColor);
            ShowGameMenu(registry, context, pause.menu, screenCenterX, screenCenterY + kMenuTopOffsetY, kMenuLabels, pause.cursorIndex);
        }

        //-------------------------------------------------------------
        //! @brief  メニューを隠す
        //! @param  registry [in] ECSレジストリ
        //! @param  pause    [in] 対象のポーズメニュー
        //-------------------------------------------------------------
        void HideUi(Tsukino::ECS::Registry& registry, const PauseMenuComponent& pause) {
            HideUiSprite(registry, pause.backdropEntity);
            HideUiText(registry, pause.titleEntity);
            HideGameMenu(registry, pause.menu);
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
                // Escで開く。スキル選択中はメニュー同士が重なるので開かない。
                // 走行が終わった後はリザルトがリトライ／タイトルへの道を持っているので開かない
                //-------------------------------------------------------------
                const bool windowFocused = !ctx->window || ctx->window->IsFocused();
                if(!windowFocused || !input.IsKeyPressed(Tsukino::Input::KeyCode::Escape))
                    continue;
                if(IsSkillSelectActive(registry) || IsRunEnded(registry))
                    continue;

                pause.isOpen          = true;
                pause.cursorIndex     = 0;
                pause.openedThisFrame = true;
                PlaySound(registry, SoundId::MenuConfirm);

                // 止めている間にヒットストップが残ると、再開した後もスローが続いてしまう
                ClearAllHitStop(registry);
                SuppressAllMoveInput(registry);
                RefreshUi(registry, *ctx, pause);
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
            // カーソル移動と決定。Escをもう一度押しても再開する
            //-------------------------------------------------------------
            const int step = ReadGameMenuStep(input);
            if(step != 0) {
                const int nextIndex = std::clamp(pause.cursorIndex + step, 0, static_cast<int>(PauseMenuItem::Count) - 1);
                if(nextIndex != pause.cursorIndex) {
                    pause.cursorIndex = nextIndex;
                    PlaySound(registry, SoundId::MenuMove);
                    RefreshUi(registry, *ctx, pause);
                }
            }

            const bool resumeByEscape = input.IsKeyPressed(Tsukino::Input::KeyCode::Escape);
            if(!resumeByEscape && !IsGameMenuConfirmPressed(input))
                continue;

            const PauseMenuItem selected = resumeByEscape ? PauseMenuItem::Resume : static_cast<PauseMenuItem>(pause.cursorIndex);
            PlaySound(registry, SoundId::MenuConfirm);

            switch(selected) {
            case PauseMenuItem::Retry:
                // ChangeScene()は次のシーンを予約するだけで、実際の切り替えは次フレーム頭
                //（GameSceneManager::Update）で行われるため、System内から直接呼んでよい
                if(ctx->gameSceneManager)
                    ctx->gameSceneManager->ChangeScene(std::make_unique<CombatAndroid::CombatAndroidScene>());
                break;

            case PauseMenuItem::Title:
                if(ctx->gameSceneManager)
                    ctx->gameSceneManager->ChangeScene(std::make_unique<CombatAndroid::TitleScene>());
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
