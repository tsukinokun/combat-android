//-------------------------------------------------------------
//! @file   RunClockSystem.cpp
//! @brief  RunClockSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/RunClockSystem.hpp>

#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Event/GameLogEvent.hpp>
#include <CombatAndroid/ECS/Utility/EnemyDifficultyTable.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>

#ifdef _DEBUG
#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#endif

#include <entt/entt.hpp>

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void RunClockSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>();

        auto view = registry.View<PlayerComponent, HealthComponent, RunClockComponent>();
        for(auto entity : view) {
            const HealthComponent& health = view.get<HealthComponent>(entity);
            RunClockComponent&     clock  = view.get<RunClockComponent>(entity);

            //-------------------------------------------------------------
            // 死んでいる間は止める。これは元々PlayerHudSystemが生存時間に
            // 掛けていた条件で、GAME OVER後にタイマーだけ進み続けないようにするもの。
            // deltaTimeはシーンがスキル選択中に0を渡してくるため、メニュー表示中も
            // 自動的に止まる（CombatAndroidSceneのscaledDeltaTime）
            //-------------------------------------------------------------
            if(!health.isDead)
                clock.elapsedSeconds += deltaTime;

#ifdef _DEBUG
            //-------------------------------------------------------------
            // F12：1分ぶん早送りする。危険度の確認を実時間で待たずに済ませるための
            // デバッグ操作。F1〜F4は負荷試験、F5はエンジンのDebugCameraSystem、
            // F6〜F11はグリップ調整が使っているためF12を充てている
            //-------------------------------------------------------------
            if(auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>()) {
                if(ctx->inputSystem && ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F12))
                    clock.elapsedSeconds += kDangerRankIntervalSeconds;
            }
#endif

            if(clock.rankUpFlashTimer > 0.0f)
                clock.rankUpFlashTimer -= deltaTime;

            //-------------------------------------------------------------
            // ランクの更新。下がることは無いので上がった時だけ演出と通知を出す
            // （F12で複数段飛んだ場合も、行は最新のランク1本だけ出る）
            //-------------------------------------------------------------
            const int newRank = GetDangerRank(clock.elapsedSeconds);
            if(newRank > clock.dangerRank) {
                clock.dangerRank       = newRank;
                clock.rankUpFlashTimer = kRankUpFlashDuration;

                if(eventBus)
                    eventBus->Publish(GameLogEvent{GameLogCategory::DangerRankUp, L"危険度 " + std::to_wstring(newRank)});
            }
        }
    }
}    // namespace CombatAndroid::ECS
