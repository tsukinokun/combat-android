//-------------------------------------------------------------
//! @file    SlowMotionController.hpp
//! @brief   大技のインパクトで世界の時間を遅くする制御の宣言
//! @note    シーンに渡すdeltaTimeそのものに倍率を掛けるので、アニメーション・敵AI・物理・
//!          エフェクト・草・霧まで世界全体が一律に遅くなる（ヒットストップは当たった
//!          エンティティだけ、こちらは画面全体）。倍率はシーンへ渡す前に決める必要があり、
//!          縮んだdeltaTimeしか受け取れないシステムでは実装できないため、シーンが持って
//!          実時間で進める
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Event/PlayerFinisherEvent.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  SlowMotionController
    //! @brief  PlayerFinisherEventを受けて、インパクトの瞬間から世界の時間を遅くする。
    //!         インパクトまで待つ → 素早く遅くなる → 保つ → 滑らかに戻る、の順に進む
    //-------------------------------------------------------------
    class SlowMotionController {
    public:
        //-------------------------------------------------------------
        // 調整値（実時間・秒）
        //-------------------------------------------------------------
        float slowScale       = 0.2f;     //!< 遅くなりきったときの時間の倍率
        float easeInDuration  = 0.05f;    //!< 倍率1から slowScale まで下げる時間
        float holdDuration    = 0.25f;    //!< slowScale のまま保つ時間
        float easeOutDuration = 0.3f;     //!< slowScale から倍率1まで戻す時間

        //-------------------------------------------------------------
        //! @brief PlayerFinisherEventの購読を開始する
        //! @param eventBus [in] シーンのイベントバス
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

        //-------------------------------------------------------------
        //! @brief  スローの進行を実時間で進め、このフレームの時間の倍率を返す
        //! @param  realDeltaTime [in] 前フレームからの実時間の経過秒（シーンが受け取った値）
        //! @return シーンへ渡すdeltaTimeに掛ける倍率（0より大きく1以下）
        //-------------------------------------------------------------
        float Advance(float realDeltaTime);

    private:
        //-------------------------------------------------------------
        //! @brief 大技通知のハンドラ。インパクトまでの待機を始める
        //! @param event [in] 大技イベント
        //-------------------------------------------------------------
        void OnPlayerFinisher(const PlayerFinisherEvent& event);

        //-------------------------------------------------------------
        //! @brief  スローを始める（保持の頭から）。既にスロー中なら倍率を途切れさせずに入り直す
        //-------------------------------------------------------------
        void Start();

        //-------------------------------------------------------------
        //! @brief  スロー開始からの経過時間に対応する倍率を返す
        //! @param  elapsed [in] スロー開始からの経過実時間（秒）
        //! @return 時間の倍率
        //-------------------------------------------------------------
        float ScaleAt(float elapsed) const;

        float m_pendingImpactDelay = -1.0f;    //!< インパクトまでの残りゲーム内時間（秒）。負なら待っていない
        float m_elapsed            = 0.0f;     //!< スロー開始からの経過実時間（秒）
        bool  m_active             = false;    //!< スロー中か

        Tsukino::ECS::ScopedConnection m_finisherConnection;    //!< PlayerFinisherEventの購読
    };
}    // namespace CombatAndroid::ECS
