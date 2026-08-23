//-------------------------------------------------------------
//! @file    EnemyStressTestSystem.hpp
//! @brief   敵の大量スポーンによる負荷試験システムの宣言
//! @author  山﨑愛
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/DebugTools/DebugFeatures.hpp>

#ifdef TSUKINO_ENABLE_STRESS_TEST

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <vector>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyStressTestSystem
    //! @brief  敵を段階的に大量スポーンし、フレーム時間の内訳をHUDへ出すシステム
    //!
    //! @note   操作（DebugFeatures.hpp の TSUKINO_ENABLE_STRESS_TEST で有効化）
    //!           F1 : 敵数を段階巡回（0→10→50→100→200→500→1000→2000→0）
    //!           F2 : HUDの表示/非表示
    //!           F3 : VSyncの切り替え（計測時はOFFにする）
    //!           F4 : 配置の切り替え（密集 / 散開）
    //!
    //!         F5はコリジョン描画、F6〜F11は武器の握り調整で使用済みのため避けている。
    //!
    //!         計測はReleaseビルドで行うこと。Debugは optimize "Off" かつ
    //!         ENTT_ASSERT が生きているためCPU時間が実態と桁で乖離する
    //-------------------------------------------------------------
    class EnemyStressTestSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief システムの更新
        //! @param registry  [in] エンティティレジストリ
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief 自動ベンチマークの実行を予約する関数
        //! @note  コマンドライン引数 --stress-benchmark を見たWinMainが呼ぶ。
        //!        シーンより先に呼ばれるため、System側は最初のUpdateで拾う。
        //!        全段階を順に回してCSVへ書き出し、終わったらアプリを終了する
        //-------------------------------------------------------------
        static void RequestAutoBenchmark();

    private:
        //-------------------------------------------------------------
        //! @enum  BenchmarkPhase
        //! @brief 自動ベンチマークの進行状態
        //-------------------------------------------------------------
        enum class BenchmarkPhase {
            Idle,       //!< 実行していない
            Warmup,     //!< 生成直後の安定待ち（この間の値は捨てる）
            Measure,    //!< 計測中
        };

        //-------------------------------------------------------------
        //! @brief 自動ベンチマークを進める関数
        //! @param registry  [in] エンティティレジストリ
        //! @param context   [in] エンジンコンテキスト
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void TickAutoBenchmark(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, float deltaTime);

        //-------------------------------------------------------------
        //! @brief 計測結果を1行CSVへ書き出す関数
        //! @param context [in] エンジンコンテキスト
        //-------------------------------------------------------------
        void WriteBenchmarkRow(Tsukino::EngineIntegration::EngineContext& context);

        //-------------------------------------------------------------
        //! @brief 現在の湧き数を指定数へ合わせる関数
        //! @param registry     [in] エンティティレジストリ
        //! @param context      [in] エンジンコンテキスト
        //! @param targetCount  [in] 目標の敵数
        //-------------------------------------------------------------
        void Respawn(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, int targetCount);

        //-------------------------------------------------------------
        //! @brief 本システムが湧かせた敵をすべて破棄する関数
        //! @param registry [in] エンティティレジストリ
        //! @note  シーンが最初から持っている4体には手を触れない
        //-------------------------------------------------------------
        void DespawnAll(Tsukino::ECS::Registry& registry);

        //-------------------------------------------------------------
        //! @brief HUDの文字列を組み立てて反映する関数
        //! @param registry         [in] エンティティレジストリ
        //! @param context          [in] エンジンコンテキスト
        //! @param toggleVisibility [in] 表示/非表示を反転させるか（F2が押されたフレームだけtrue）
        //-------------------------------------------------------------
        void UpdateHud(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, bool toggleVisibility);

        //! @brief F1で巡回する敵数の段階
        //! @note  最適化後は500体でもまだ60fpsに余裕があったため、上限側を足して
        //!        実際に頭打ちになる地点を測れるようにしている
        static constexpr int kCountSteps[] = {0, 10, 50, 100, 200, 500, 1000, 2000};

        //! @brief 本システムが湧かせた敵の本体エンティティ（HPバー2体はHealthComponentから辿る）
        std::vector<Tsukino::ECS::Entity> m_spawned;

        int m_stepIndex = 0;    //!< kCountSteps のどこにいるか

        //! 密集配置かどうか。散開させると多くが画角外・シャドウ投影ボックス外に落ちて
        //! 偶然安くなり限界値を過大評価してしまうため、限界の判定は密集で行う
        bool m_dense = true;

        //-------------------------------------------------------------
        // 自動ベンチマーク用
        //-------------------------------------------------------------
        //! @brief 生成直後の安定待ち時間（秒）。アセットのウォームアップとパイプラインキャッシュの充填を待つ
        static constexpr float kBenchmarkWarmupSeconds = 3.0f;

        //! @brief 1段階あたりの計測時間（秒）
        static constexpr float kBenchmarkMeasureSeconds = 5.0f;

        BenchmarkPhase m_benchmarkPhase = BenchmarkPhase::Idle;

        int   m_benchmarkStepIndex = 0;      //!< 計測中の段階
        float m_benchmarkElapsed   = 0.0f;   //!< 現在のフェーズの経過時間（秒）

        double m_benchmarkFrameMsSum  = 0.0;    //!< 計測期間のフレーム時間の合計（ミリ秒）
        double m_benchmarkFrameMsWorst = 0.0;   //!< 計測期間で最も遅かったフレーム（ミリ秒）
        int    m_benchmarkFrameCount   = 0;     //!< 計測期間のフレーム数

        bool m_benchmarkStarted = false;    //!< 予約を拾って開始済みか
    };
}    // namespace CombatAndroid::ECS

#endif    // TSUKINO_ENABLE_STRESS_TEST
