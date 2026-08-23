//--------------------------------------------------------------
//! @file	WinMain.cpp
//! @brief	CombatAndroidのエントリポイント
//! @author 山﨑愛
//--------------------------------------------------------------
#include <Tsukino/EngineIntegration/EngineAPI.hpp>
#include <Tsukino/EngineIntegration/EngineIntegration.hpp>
#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/DebugTools/DebugFeatures.hpp>
#include <Tsukino/Core/DebugTools/FrameProfiler.hpp>
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>
#ifdef TSUKINO_ENABLE_STRESS_TEST
#include <CombatAndroid/ECS/System/EnemyStressTestSystem.hpp>
#endif

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <memory>

//--------------------------------------------------------------
// アプリケーションのエントリポイント
//! @param hInstance アプリケーションインスタンス
//! @param hPrevInstance 非推奨（常にNULL）
//! @param lpCmdLine コマンドライン引数
//! @param nCmdShow ウィンドウ表示状態（例：SW_SHOW）
//! @return 終了コード（通常は0）
//--------------------------------------------------------------
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR lpCmdLine, _In_ int) {
    // DPIスケーリングの無効化
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#ifdef TSUKINO_ENABLE_STRESS_TEST
    //--------------------------------------------------------------
    // --stress-benchmark 付きで起動されたら、敵数を段階的に上げながら
    // 自動計測してStressTestResult.csvへ書き出し、終わり次第終了する。
    // 手元で数字を取り直すとき、毎回ホットキーを押して回らなくて済むようにするためのもの
    //--------------------------------------------------------------
    if(lpCmdLine != nullptr && std::strstr(lpCmdLine, "--stress-benchmark") != nullptr) {
        CombatAndroid::ECS::EnemyStressTestSystem::RequestAutoBenchmark();
    }
#else
    (void)lpCmdLine;
#endif
    // ログの初期化
    Tsukino::EngineIntegration::EngineIntegration engineIntegration;
    // 初期化
    if(!engineIntegration.Initialize(1700, 1000)) {
        // 初期化に失敗した場合はエラーログを出力して終了
        Tsukino::Core::Log::Error("Failed to initialize EngineIntegration.");
        return EXIT_FAILURE;
    }

    Tsukino::EngineIntegration::EngineContext& engineContext = engineIntegration.GetContext();
    Tsukino::EngineIntegration::EngineAPI      engineAPI(engineContext);

    //--------------------------------------------------------------
    // 最初のシーンを登録・開始
    //--------------------------------------------------------------
    engineAPI.ChangeScene(std::make_unique<CombatAndroid::CombatAndroidScene>());

    //--------------------------------------------------------------
    // メインループ
    // deltaTimeは実測の経過時間を使う。固定値（1/60）だとPresent(1,0)のVSyncが
    // 60Hzより高いリフレッシュレートのディスプレイに同期した場合、実際のフレーム間隔より
    // 大きい値としてシミュレーションが進んでしまい、アニメーション等が実際より速く再生される
    //--------------------------------------------------------------
    auto lastTime = std::chrono::steady_clock::now();

    namespace DebugTools = Tsukino::Core::DebugTools;
    DebugTools::FrameProfiler& profiler = DebugTools::FrameProfiler::Get();

    while(engineAPI.ProcessMessages()) {
        auto  currentTime = std::chrono::steady_clock::now();
        float deltaTime    = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime            = currentTime;
        // ウィンドウドラッグ等で1フレームが極端に長くなった場合の暴走を防ぐ上限
        deltaTime = std::min(deltaTime, 1.0f / 15.0f);

        //----------------------------------------------------------
        // フレーム時間の計測（負荷調査用）
        // Update（全システム）と Render（描画コマンドのサブミット）を分けて測る。
        // 両者の合計がFrameに満たない場合、その差はPresentのVSync待ちである
        //----------------------------------------------------------
        profiler.AddSample(DebugTools::ProfileSlot::Frame, static_cast<double>(deltaTime) * 1000.0);

        {
            // 一括更新
            DebugTools::ScopedProfileTimer updateTimer(DebugTools::ProfileSlot::Update);
            engineAPI.Update(deltaTime);
        }
        {
            // 描画処理
            DebugTools::ScopedProfileTimer renderTimer(DebugTools::ProfileSlot::Render);
            engineAPI.Render();
        }

        profiler.Tick(deltaTime);
    }

    //--------------------------------------------------------------
    // ウィンドウは自動的に破棄される
    //--------------------------------------------------------------
    return 0;
}
