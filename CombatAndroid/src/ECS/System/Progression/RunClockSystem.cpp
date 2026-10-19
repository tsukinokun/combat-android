//-------------------------------------------------------------
//! @file   RunClockSystem.cpp
//! @brief  RunClockSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Progression/RunClockSystem.hpp>

#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Event/UI/GameLogEvent.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/System/Progression/BossDirectorSystem.hpp>
#include <CombatAndroid/ECS/Utility/Table/EnemyDifficultyTable.hpp>

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
            // 自動的に止まる（CombatAndroidSceneのscaledDeltaTime）。
            // 上限は無い（クリアは時間ではなくラスボスの撃破で決まる）
            // ボス戦の間（撃破の演出待ちを含む）も止める。危険度はこの時計から決まるので
            // 一緒に止まり、ラスボスまでの待ちも「中ボスを倒した時刻」から数え直しになる。
            // ムービー（ライブのカットシーンは時間が進む）の間も止める
            //-------------------------------------------------------------
            if(!health.isDead && !IsBossBattleActive(registry) && !IsCutsceneActive(registry))
                clock.elapsedSeconds += deltaTime;

#ifdef _DEBUG
            //-------------------------------------------------------------
            // F12：1分ぶん早送りする。危険度の確認を実時間で待たずに済ませるための
            // デバッグ操作。F1〜F4は負荷試験、F5はエンジンのDebugCameraSystem、
            // F6〜F11はグリップ調整が使っているためF12を充てている。
            // Shift+F12はボスを倒す操作（BossDirectorSystem）なので早送りしない
            //-------------------------------------------------------------
            if(auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>()) {
                if(ctx->inputSystem && ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F12)
                   && !ctx->inputSystem->IsKeyDown(Tsukino::Input::KeyCode::Shift))
                    clock.elapsedSeconds += GetDangerRankIntervalSeconds();
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
