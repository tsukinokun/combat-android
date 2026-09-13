//-------------------------------------------------------------
//! @file    SlowMotionController.cpp
//! @brief   大技のインパクトで世界の時間を遅くする制御の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/SlowMotionController.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @brief 0〜1の進み具合を、始めと終わりがなだらかな曲線に写す
        //-------------------------------------------------------------
        float SmoothStep(float t) {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief PlayerFinisherEventの購読を開始する
    //-------------------------------------------------------------
    void SlowMotionController::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_finisherConnection =
            eventBus.Subscribe<PlayerFinisherEvent>([this](const PlayerFinisherEvent& event) { OnPlayerFinisher(event); });
    }

    //-------------------------------------------------------------
    //! @brief 大技通知のハンドラ
    //-------------------------------------------------------------
    void SlowMotionController::OnPlayerFinisher(const PlayerFinisherEvent& event) {
        const float delay = std::max(event.impactDelay, 0.0f);

        // 待機中にもう1発来たら、先に来るインパクトに合わせる
        m_pendingImpactDelay = (m_pendingImpactDelay < 0.0f) ? delay : std::min(m_pendingImpactDelay, delay);
    }

    //-------------------------------------------------------------
    //! @brief スロー開始からの経過時間に対応する倍率を返す
    //-------------------------------------------------------------
    float SlowMotionController::ScaleAt(float elapsed) const {
        const float easeIn  = std::max(easeInDuration, 0.0f);
        const float hold    = std::max(holdDuration, 0.0f);
        const float easeOut = std::max(easeOutDuration, 0.0f);

        if(elapsed < easeIn)
            return 1.0f + (slowScale - 1.0f) * SmoothStep(elapsed / easeIn);

        if(elapsed < easeIn + hold)
            return slowScale;

        if(elapsed < easeIn + hold + easeOut)
            return slowScale + (1.0f - slowScale) * SmoothStep((elapsed - easeIn - hold) / easeOut);

        return 1.0f;
    }

    //-------------------------------------------------------------
    //! @brief スローを始める
    //-------------------------------------------------------------
    void SlowMotionController::Start() {
        const float easeIn = std::max(easeInDuration, 0.0f);

        if(!m_active || m_elapsed < easeIn) {
            // 新しく始める。入りの途中ならそのまま入りを続ける
            if(!m_active)
                m_elapsed = 0.0f;
        } else if(m_elapsed < easeIn + std::max(holdDuration, 0.0f)) {
            // 保持の途中なら、保持の頭からやり直す（倍率は同じなので途切れない）
            m_elapsed = easeIn;
        } else {
            //-------------------------------------------------------------
            // 戻りの途中。いきなり slowScale へ跳ぶと画面が一瞬で止まって見えるので、
            // 入りの曲線の中から「今と同じ倍率になる位置」を探してそこから入り直す。
            // 入りの曲線は単調に下がるので二分探索で求まる
            //-------------------------------------------------------------
            const float currentScale = ScaleAt(m_elapsed);

            float low  = 0.0f;
            float high = easeIn;
            for(int i = 0; i < 16; ++i) {
                const float mid = (low + high) * 0.5f;
                if(ScaleAt(mid) > currentScale)
                    low = mid;
                else
                    high = mid;
            }
            m_elapsed = (low + high) * 0.5f;
        }

        m_active = true;
    }

    //-------------------------------------------------------------
    //! @brief スローの進行を実時間で進め、このフレームの時間の倍率を返す
    //-------------------------------------------------------------
    float SlowMotionController::Advance(float realDeltaTime) {
        const float dt = std::max(realDeltaTime, 0.0f);

        //-------------------------------------------------------------
        // インパクトまでの待機はゲーム内時間で数える（範囲攻撃・斬撃弾の発動はゲーム内時間で
        // 数えられているため）。スロー中に次の大技が来ても、インパクトの瞬間にちゃんと合う
        //-------------------------------------------------------------
        if(m_pendingImpactDelay >= 0.0f) {
            const float currentScale = m_active ? ScaleAt(m_elapsed) : 1.0f;

            m_pendingImpactDelay -= dt * currentScale;
            if(m_pendingImpactDelay <= 0.0f) {
                m_pendingImpactDelay = -1.0f;
                Start();
            }
        }

        if(!m_active)
            return 1.0f;

        const float scale = ScaleAt(m_elapsed);

        m_elapsed += dt;
        if(m_elapsed >= std::max(easeInDuration, 0.0f) + std::max(holdDuration, 0.0f) + std::max(easeOutDuration, 0.0f))
            m_active = false;

        return scale;
    }
}    // namespace CombatAndroid::ECS
