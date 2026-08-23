//-------------------------------------------------------------
//! @file    EnemyStressTestSystem.cpp
//! @brief   敵の大量スポーンによる負荷試験システムの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/EnemyStressTestSystem.hpp>

#ifdef TSUKINO_ENABLE_STRESS_TEST

#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyStressTestComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Renderer/Renderer.hpp>

#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>

#include <Tsukino/Core/DebugTools/FrameProfiler.hpp>
#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Log.hpp>

#include <entt/entt.hpp>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 円周率
        constexpr float kPi = 3.14159265f;

        //! 密集配置：全個体が画角内かつシャドウ投影ボックス（原点±500）内に収まる範囲へ詰める。
        //! 最悪ケース（カリングも投影クリップも効かない状態）を作るのが狙い
        constexpr float kDenseInnerRadius = 250.0f;
        constexpr float kDenseRingPitch   = 55.0f;
        constexpr int   kDenseRingCount   = 5;

        //! 散開配置：画角外・シャドウ投影ボックス外へも散らし、カリングの効き目を見るための配置
        constexpr float kSpreadInnerRadius = 400.0f;
        constexpr float kSpreadRingPitch   = 220.0f;

        //! 地面から少し浮かせて出す（生成直後に床へめり込まないように）
        constexpr float kSpawnHeight = 20.0f;

        //! 自動ベンチマークの結果を書き出すファイル名（カレントディレクトリ基準）
        constexpr const char* kBenchmarkCsvPath = "StressTestResult.csv";

        //! 自動ベンチマークが予約されたか。
        //! WinMainがコマンドライン引数を見てSystemの生成より先に立てるため、
        //! インスタンスのメンバではなく静的変数で受ける
        bool s_AutoBenchmarkRequested = false;

        //-------------------------------------------------------------
        //! @brief  i番目の敵の出現位置を求める関数
        //! @param  index [in] 通し番号
        //! @param  dense [in] 密集配置か
        //! @return 出現位置
        //! @note   同心円上へ等間隔に並べる。1周あたりの数を半径に比例させることで
        //!         密度を一定に保ち、全員が1点へ重なってJoltのブロードフェーズの
        //!         同じセルに入ってしまうのを避ける
        //-------------------------------------------------------------
        [[nodiscard]]
        hlslpp::float3 ResolveSpawnPosition(int index, bool dense) {
            const float innerRadius = dense ? kDenseInnerRadius : kSpreadInnerRadius;
            const float ringPitch   = dense ? kDenseRingPitch : kSpreadRingPitch;

            // 1周あたりの数（半径が大きいほど多く並ぶ）
            int ring          = 0;
            int remaining     = index;
            int countInRing   = 12;
            while(remaining >= countInRing) {
                remaining -= countInRing;
                ++ring;
                countInRing += 6;
            }

            // 密集配置は指定リング数で折り返し、外へ広がりすぎないようにする
            const int effectiveRing = dense ? (ring % kDenseRingCount) : ring;

            const float radius = innerRadius + ringPitch * static_cast<float>(effectiveRing);

            // リングごとに位相をずらして格子状に重ならないようにする
            const float angle = (2.0f * kPi * static_cast<float>(remaining) / static_cast<float>(countInRing)) + static_cast<float>(ring) * 0.37f;

            return hlslpp::float3(std::cos(angle) * radius, kSpawnHeight, std::sin(angle) * radius);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyStressTestSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->inputSystem)
            return;

        //---------------------------------------------------------
        // 自動ベンチマークの予約を拾う（--stress-benchmark 付きで起動された場合）
        //---------------------------------------------------------
        if(s_AutoBenchmarkRequested && !m_benchmarkStarted) {
            m_benchmarkStarted   = true;
            m_benchmarkPhase     = BenchmarkPhase::Warmup;
            m_benchmarkStepIndex = 0;
            m_benchmarkElapsed   = 0.0f;

            // 計測はVSyncを切って行う（有効なままだとリフレッシュレートで頭打ちになる）
            if(ctx->renderer)
                ctx->renderer->SetVSyncEnabled(false);

            Respawn(registry, *ctx, kCountSteps[m_benchmarkStepIndex]);
            Tsukino::Core::Log::Info("EnemyStressTest: auto benchmark started");
        }

        if(m_benchmarkPhase != BenchmarkPhase::Idle) {
            TickAutoBenchmark(registry, *ctx, deltaTime);
            UpdateHud(registry, *ctx, false);
            return;
        }

        //---------------------------------------------------------
        // F1：敵数を段階巡回する
        // 生成・破棄はViewの反復の外側（ここ）でのみ行う。
        // 反復中にエンティティを増減させるとEnTTのイテレータが壊れる
        //---------------------------------------------------------
        if(ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F1)) {
            m_stepIndex = (m_stepIndex + 1) % static_cast<int>(std::size(kCountSteps));
            Respawn(registry, *ctx, kCountSteps[m_stepIndex]);

            Tsukino::Core::Log::Info("EnemyStressTest: enemy count = " + std::to_string(kCountSteps[m_stepIndex]));
        }

        // F2：HUDの表示/非表示。実際の反映はHUDのコンポーネントを引けるUpdateHudで行う
        const bool toggleHud = ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F2);

        //---------------------------------------------------------
        // F3：VSyncの切り替え
        // VSyncが有効なままだとフレーム時間がリフレッシュレートで頭打ちになり、
        // 負荷の増減が数字に出ない。計測は必ずOFFで行う
        //---------------------------------------------------------
        if(ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F3) && ctx->renderer) {
            const bool enabled = !ctx->renderer->IsVSyncEnabled();
            ctx->renderer->SetVSyncEnabled(enabled);

            Tsukino::Core::Log::Info(enabled ? "EnemyStressTest: VSync ON" : "EnemyStressTest: VSync OFF");
        }

        //---------------------------------------------------------
        // F4：配置の切り替え（密集 / 散開）
        //---------------------------------------------------------
        if(ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F4)) {
            m_dense = !m_dense;
            Respawn(registry, *ctx, kCountSteps[m_stepIndex]);

            Tsukino::Core::Log::Info(m_dense ? "EnemyStressTest: layout DENSE" : "EnemyStressTest: layout SPREAD");
        }

        UpdateHud(registry, *ctx, toggleHud);
    }

    //-------------------------------------------------------------
    //! @brief 自動ベンチマークの実行を予約する
    //-------------------------------------------------------------
    void EnemyStressTestSystem::RequestAutoBenchmark() {
        s_AutoBenchmarkRequested = true;
    }

    //-------------------------------------------------------------
    //! @brief 自動ベンチマークを進める
    //-------------------------------------------------------------
    void EnemyStressTestSystem::TickAutoBenchmark(Tsukino::ECS::Registry& registry,
                                                  Tsukino::EngineIntegration::EngineContext& context,
                                                  float deltaTime) {
        m_benchmarkElapsed += deltaTime;

        //---------------------------------------------------------
        // 安定待ち：生成直後はアセットのウォームアップやパイプラインキャッシュの
        // 充填で重くなるため、この間の値は捨てる
        //---------------------------------------------------------
        if(m_benchmarkPhase == BenchmarkPhase::Warmup) {
            if(m_benchmarkElapsed < kBenchmarkWarmupSeconds)
                return;

            m_benchmarkPhase        = BenchmarkPhase::Measure;
            m_benchmarkElapsed      = 0.0f;
            m_benchmarkFrameMsSum   = 0.0;
            m_benchmarkFrameMsWorst = 0.0;
            m_benchmarkFrameCount   = 0;
            return;
        }

        //---------------------------------------------------------
        // 計測：フレーム時間は自前で積む（FrameProfilerの移動平均は
        // 直近0.5秒ぶんしか見ないため、計測窓全体の平均にはならない）
        //---------------------------------------------------------
        const double frameMs = static_cast<double>(deltaTime) * 1000.0;
        m_benchmarkFrameMsSum += frameMs;
        m_benchmarkFrameMsWorst = std::max(m_benchmarkFrameMsWorst, frameMs);
        ++m_benchmarkFrameCount;

        if(m_benchmarkElapsed < kBenchmarkMeasureSeconds)
            return;

        WriteBenchmarkRow(context);

        //---------------------------------------------------------
        // 次の段階へ。全段階を終えたらアプリを終了する
        //---------------------------------------------------------
        ++m_benchmarkStepIndex;
        if(m_benchmarkStepIndex >= static_cast<int>(std::size(kCountSteps))) {
            m_benchmarkPhase = BenchmarkPhase::Idle;

            // 湧かせた敵を片付けてから終了する。大量のエンティティを抱えたまま
            // プロセスを終わらせると、終了処理の負荷がそのまま乗ってしまう
            DespawnAll(registry);

            Tsukino::Core::Log::Info("EnemyStressTest: auto benchmark finished");
            ::PostQuitMessage(0);
            return;
        }

        m_benchmarkPhase   = BenchmarkPhase::Warmup;
        m_benchmarkElapsed = 0.0f;
        Respawn(registry, context, kCountSteps[m_benchmarkStepIndex]);
    }

    //-------------------------------------------------------------
    //! @brief 計測結果を1行CSVへ書き出す
    //-------------------------------------------------------------
    void EnemyStressTestSystem::WriteBenchmarkRow(Tsukino::EngineIntegration::EngineContext& context) {
        namespace DebugTools = Tsukino::Core::DebugTools;

        if(m_benchmarkFrameCount <= 0)
            return;

        const double avgFrameMs = m_benchmarkFrameMsSum / static_cast<double>(m_benchmarkFrameCount);
        const double fps        = (avgFrameMs > 0.0) ? (1000.0 / avgFrameMs) : 0.0;

        const DebugTools::FrameProfiler& profiler = DebugTools::FrameProfiler::Get();

        //---------------------------------------------------------
        // 1段階目でヘッダを書く（毎回上書きし、追記はしない）
        //---------------------------------------------------------
        const bool isFirstRow = (m_benchmarkStepIndex == 0);

        std::ofstream file(kBenchmarkCsvPath, isFirstRow ? std::ios::trunc : std::ios::app);
        if(!file)
            return;

        if(isFirstRow) {
            file << "# CombatAndroid enemy stress test\n"
                    "# frame_ms / worst_ms は計測窓全体の実測。update_ms 以降は FrameProfiler の直近0.5秒平均\n"
                    "# spawned は本システムが湧かせた数。シーン標準の敵4体は別途常駐している\n"
                    "spawned,frame_ms,fps,worst_ms,update_ms,render_ms,"
                    "draws_total,draws_gbuffer,draws_shadow,draws_overlay,commands,skinned_draws,triangles,skin_cb_mb,"
                    "systems\n";
        }

        file << kCountSteps[m_benchmarkStepIndex] << ',' << avgFrameMs << ',' << fps << ',' << m_benchmarkFrameMsWorst << ','
             << profiler.GetAverageMs(DebugTools::ProfileSlot::Update) << ',' << profiler.GetAverageMs(DebugTools::ProfileSlot::Render) << ',';

        if(context.renderer) {
            const Tsukino::Renderer::Renderer::FrameStats& stats = context.renderer->GetFrameStats();
            file << stats.TotalDrawCalls() << ',' << stats.gbufferDrawCalls << ',' << stats.shadowDrawCalls << ',' << stats.overlayDrawCalls << ','
                 << stats.commandCount << ',' << stats.skinnedDrawCalls << ',' << stats.triangleCount << ','
                 << (static_cast<double>(stats.boneBytesUploaded) / (1024.0 * 1024.0)) << ',';
        } else {
            file << ",,,,,,,,";
        }

        //---------------------------------------------------------
        // システム別の内訳は "名前=ミリ秒" を空白区切りで1セルに詰める。
        // システム数と並びが変わっても列がずれないようにするため
        //---------------------------------------------------------
        file << '"';
        for(const DebugTools::FrameProfiler::SystemTiming& timing : profiler.GetSystemTimings()) {
            if(timing.averageMs < 0.005)
                continue;

            file << ((timing.name != nullptr) ? timing.name : "?") << '=' << timing.averageMs << ' ';
        }
        file << "\"\n";

        Tsukino::Core::Log::Info("EnemyStressTest: recorded " + std::to_string(kCountSteps[m_benchmarkStepIndex]) + " enemies");
    }

    //-------------------------------------------------------------
    //! @brief 現在の湧き数を指定数へ合わせる
    //-------------------------------------------------------------
    void EnemyStressTestSystem::Respawn(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, int targetCount) {
        DespawnAll(registry);

        if(targetCount <= 0)
            return;

        //---------------------------------------------------------
        // 生成パラメータは1回だけ作って使い回す。
        // 敵の種類はSmallZombieのみに揃えている。1体あたりのコストを一定にしておかないと
        // 「敵数を増やしたときフレーム時間がどう伸びるか」の曲線が読めなくなるため
        // （BigZombieはシーン側の初期4体に含まれており、材質切り替えはそちらで発生する）
        //---------------------------------------------------------
        EnemySpawnConfig config = MakeSmallZombieConfig(context, hlslpp::float3(0.0f, 0.0f, 0.0f));

        m_spawned.reserve(m_spawned.size() + static_cast<size_t>(targetCount));

        for(int i = 0; i < targetCount; ++i) {
            config.spawnPosition = ResolveSpawnPosition(i, m_dense);

            // 全個体の再生位置をずらす。揃っているとAnimationSystemが毎フレーム
            // 完全に同じ分岐・同じキーフレームを辿ることになり、実態より軽く測れてしまう
            config.initialAnimationTime = static_cast<float>(i % 97) * 0.031f;

            m_spawned.push_back(SpawnBehaviorEnemy(registry, context, config));
        }
    }

    //-------------------------------------------------------------
    //! @brief 本システムが湧かせた敵をすべて破棄する
    //-------------------------------------------------------------
    void EnemyStressTestSystem::DespawnAll(Tsukino::ECS::Registry& registry) {
        for(Tsukino::ECS::Entity entity : m_spawned) {
            //-----------------------------------------------------
            // 敵1体につきHPバーが2エンティティぶら下がっている。
            // 本体だけ消すとHPバーが宙に浮いたまま残り、エンティティ数が戻らない
            //-----------------------------------------------------
            if(registry.HasComponent<CombatAndroid::ECS::HealthComponent>(entity)) {
                const CombatAndroid::ECS::HealthComponent& health = registry.GetComponent<CombatAndroid::ECS::HealthComponent>(entity);

                if(health.hpBarBackgroundEntity != entt::null)
                    registry.QueueDestroy(health.hpBarBackgroundEntity);

                if(health.hpBarFillEntity != entt::null)
                    registry.QueueDestroy(health.hpBarFillEntity);
            }

            // System の中からの破棄は必ず QueueDestroy を使う（即時破棄はイテレータを壊す）。
            // Joltのボディは Registry::OnDestroy<CollisionComponent> 経由で
            // PhysicsSystem が回収するため、この経路でも解放漏れは起きない
            registry.QueueDestroy(entity);
        }

        m_spawned.clear();
    }

    //-------------------------------------------------------------
    //! @brief HUDの文字列を組み立てて反映する
    //-------------------------------------------------------------
    void EnemyStressTestSystem::UpdateHud(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, bool toggleVisibility) {
        namespace DebugTools = Tsukino::Core::DebugTools;

        auto hudView = registry.View<CombatAndroid::ECS::EnemyStressTestComponent, Tsukino::BuiltIn::ECS::FontComponent>();

        hudView.each([&](entt::entity, CombatAndroid::ECS::EnemyStressTestComponent& hud, Tsukino::BuiltIn::ECS::FontComponent& font) {
            if(toggleVisibility)
                hud.visible = !hud.visible;

            if(!hud.visible) {
                // 空文字の間はFontRendererSystemが描画しない
                font.text.clear();
                return;
            }

            const DebugTools::FrameProfiler& profiler = DebugTools::FrameProfiler::Get();

            const double frameMs  = profiler.GetAverageMs(DebugTools::ProfileSlot::Frame);
            const double updateMs = profiler.GetAverageMs(DebugTools::ProfileSlot::Update);
            const double renderMs = profiler.GetAverageMs(DebugTools::ProfileSlot::Render);
            const double fps      = (frameMs > 0.0) ? (1000.0 / frameMs) : 0.0;

            const bool vsync = context.renderer && context.renderer->IsVSyncEnabled();

            //-----------------------------------------------------
            // 概要行
            //-----------------------------------------------------
            wchar_t buffer[512]{};
            int     written = swprintf_s(buffer,
                                     L"[F1]Enemies %d  [F2]HUD  [F3]VSync %s  [F4]Layout %s\n"
                                     L"Frame %6.2f ms (%5.1f fps)   Update %6.2f   Render %6.2f\n",
                                     kCountSteps[m_stepIndex],
                                     vsync ? L"ON" : L"OFF",
                                     m_dense ? L"DENSE" : L"SPREAD",
                                     frameMs,
                                     fps,
                                     updateMs,
                                     renderMs);

            if(written < 0)
                return;

            std::wstring text(buffer, static_cast<size_t>(written));

            //-----------------------------------------------------
            // 描画統計
            // フレーム時間だけでは「ドローが多いのか1本が重いのか」が分からないため内訳を出す。
            // SkinCBは実ボーン数に関係なく1ドローあたり8KB転送している総量
            //-----------------------------------------------------
            if(context.renderer) {
                const Tsukino::Renderer::Renderer::FrameStats& stats = context.renderer->GetFrameStats();

                written = swprintf_s(buffer,
                                     L"Draws %u (GB %u / Shadow %u / Overlay %u / Other %u)  Cmds %u\n"
                                     L"Skinned %u   Tris %.2fM   SkinCB %.2f MB/f\n",
                                     stats.TotalDrawCalls(),
                                     stats.gbufferDrawCalls,
                                     stats.shadowDrawCalls,
                                     stats.overlayDrawCalls,
                                     stats.worldDrawCalls + stats.transparentDrawCalls + stats.waterDrawCalls,
                                     stats.commandCount,
                                     stats.skinnedDrawCalls,
                                     static_cast<double>(stats.triangleCount) / 1000000.0,
                                     static_cast<double>(stats.boneBytesUploaded) / (1024.0 * 1024.0));

                if(written > 0)
                    text.append(buffer, static_cast<size_t>(written));
            }

            //-----------------------------------------------------
            // システム別のCPU時間（重い順に上位のみ）。
            // どこが重いのかを推測でなく数字で切り分けるための本命の表示
            //-----------------------------------------------------
            const std::vector<DebugTools::FrameProfiler::SystemTiming>& timings = profiler.GetSystemTimings();

            // 重い順の上位kTopSystemCount件だけを出す。
            // ポインタの配列を部分ソートするだけなので、毎フレームのヒープ確保は起きない
            constexpr size_t kTopSystemCount = 6;
            constexpr size_t kMaxSystemCount = 64;
            constexpr double kNegligibleMs   = 0.005;

            std::array<const DebugTools::FrameProfiler::SystemTiming*, kMaxSystemCount> ranked{};

            const size_t rankedCount = std::min(timings.size(), kMaxSystemCount);
            for(size_t i = 0; i < rankedCount; ++i) {
                ranked[i] = &timings[i];
            }

            const size_t topCount = std::min(kTopSystemCount, rankedCount);
            std::partial_sort(ranked.begin(),
                              ranked.begin() + topCount,
                              ranked.begin() + rankedCount,
                              [](const DebugTools::FrameProfiler::SystemTiming* a, const DebugTools::FrameProfiler::SystemTiming* b) {
                                  return a->averageMs > b->averageMs;
                              });

            for(size_t i = 0; i < topCount; ++i) {
                // 0に近いシステムまで並べても読めないので打ち切る
                if(ranked[i]->averageMs < kNegligibleMs)
                    break;

                written = swprintf_s(buffer, L"  %-24hs %6.2f ms\n", (ranked[i]->name != nullptr) ? ranked[i]->name : "?", ranked[i]->averageMs);
                if(written > 0)
                    text.append(buffer, static_cast<size_t>(written));
            }

            font.text = std::move(text);
        });
    }
}    // namespace CombatAndroid::ECS

#endif    // TSUKINO_ENABLE_STRESS_TEST
