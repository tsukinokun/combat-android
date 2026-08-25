//-------------------------------------------------------------
//! @file   GameOverSystem.cpp
//! @brief  GameOverSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/GameOverSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/GameOverComponent.hpp>
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>

#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

#include <entt/entt.hpp>

#include <memory>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! @brief 死亡モーションの再生が落ち着く頃合い（実測ではなく見た目基準の暫定値。
        //!        Falling Back Death.fbxの長さに応じて実機で調整する）を待ってからGAME OVERを出す
        constexpr float kOverlayDelay = 1.5f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void GameOverSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        auto view = registry.View<PlayerComponent, HealthComponent, GameOverComponent>();
        for(entt::entity entity : view) {
            auto& health    = view.get<HealthComponent>(entity);
            auto& gameOver = view.get<GameOverComponent>(entity);

            if(!health.isDead) {
                // 死亡していない（生存中、あるいはまだシーンが始まったばかり）。次の死亡に備えて状態を戻す
                gameOver.deathElapsed = 0.0f;
                gameOver.overlayShown = false;
                continue;
            }

            gameOver.deathElapsed += deltaTime;

            if(!gameOver.overlayShown) {
                if(gameOver.deathElapsed < kOverlayDelay)
                    continue;    // まだ死亡モーションの再生を見せたい時間帯

                gameOver.overlayShown = true;

                if(gameOver.titleTextEntity != entt::null) {
                    if(auto* titleFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(gameOver.titleTextEntity))
                        titleFont->text = L"GAME OVER";
                }
                if(gameOver.retryTextEntity != entt::null) {
                    if(auto* retryFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(gameOver.retryTextEntity))
                        retryFont->text = L"Press SPACE to Retry";
                }

                continue;    // 表示した直後のフレームでそのままリトライ入力を拾わない
            }

            //-------------------------------------------------------------
            // GAME OVER表示中。スペースキーでシーンを作り直す。
            // ChangeScene()は次のシーンを予約するだけで、実際の切り替えは次フレーム頭
            // （GameSceneManager::Update）で行われるため、System内から直接呼んでよい
            //-------------------------------------------------------------
            if(ctx->inputSystem && ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::Space) && ctx->gameSceneManager) {
                ctx->gameSceneManager->ChangeScene(std::make_unique<CombatAndroid::CombatAndroidScene>());
            }
        }
    }
}    // namespace CombatAndroid::ECS
