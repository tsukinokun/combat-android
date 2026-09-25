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

#include <algorithm>
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
            const float previousSeconds = clock.elapsedSeconds;
            if(!health.isDead)
                clock.elapsedSeconds = std::min(clock.elapsedSeconds + deltaTime, kRunClearSeconds);    // クリアの瞬間で止める

#ifdef _DEBUG
            //-------------------------------------------------------------
            // F12：1分ぶん早送りする。危険度の確認を実時間で待たずに済ませるための
            // デバッグ操作。F1〜F4は負荷試験、F5はエンジンのDebugCameraSystem、
            // F6〜F11はグリップ調整が使っているためF12を充てている
            //-------------------------------------------------------------
            if(auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>()) {
                if(ctx->inputSystem && ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F12))
                    clock.elapsedSeconds += GetDangerRankIntervalSeconds();
            }
#endif

            if(clock.rankUpFlashTimer > 0.0f)
                clock.rankUpFlashTimer -= deltaTime;

            //-------------------------------------------------------------
            // ラスト1分に入った瞬間を知らせる。ここから湧きが強まる（EnemySpawnDirectorSystem）ので、
            // 危険度の上昇と同じ行で「山場に入った」ことを伝える
            //-------------------------------------------------------------
            const float finalStretchStart = kRunClearSeconds - kRunFinalStretchSeconds;
            if(previousSeconds < finalStretchStart && clock.elapsedSeconds >= finalStretchStart && eventBus)
                eventBus->Publish(GameLogEvent{GameLogCategory::DangerRankUp, L"ラスト1分"});

            //-------------------------------------------------------------
            // ランクの更新。下がることは無いので上がった時だけ演出と通知を出す
            // （F12で複数段飛んだ場合も、行は最新のランク1本だけ出る）
            //-------------------------------------------------------------
            // クリアの瞬間（kRunClearSeconds）は危険度テーブル上ちょうど次の段の始まりに当たるが、
            // その段で戦うことは無いので上げない（リザルトに「危険度11」と出さないため）
            const int newRank = GetDangerRank(std::min(clock.elapsedSeconds, kRunClearSeconds - 0.001f));
            if(newRank > clock.dangerRank) {
                clock.dangerRank       = newRank;
                clock.rankUpFlashTimer = kRankUpFlashDuration;

                if(eventBus)
                    eventBus->Publish(GameLogEvent{GameLogCategory::DangerRankUp, L"危険度 " + std::to_wstring(newRank)});
            }
        }
    }
}    // namespace CombatAndroid::ECS
