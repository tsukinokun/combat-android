//-------------------------------------------------------------
//! @file   TpsCameraSystem.hpp
//! @brief  TpsCameraSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/PlayerDamagedEvent.hpp>
#include <CombatAndroid/ECS/Event/PlayerFinisherEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <random>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  TpsCameraSystem
    //! @brief  プレイヤーの向いている方向の後方に追従する三人称視点カメラの更新システム
    //! @note   位置はばねで追従し、PlayerDamagedEventを受けると注視点をばねで揺らす。
    //!         PlayerFinisherEventを受けると、プレイヤーへ寄って画角を狭める
    //-------------------------------------------------------------
    class TpsCameraSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief PlayerDamagedEvent・PlayerFinisherEventの購読を開始する
        //! @param eventBus [in] シーンのイベントバス
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief 被弾通知のハンドラ。揺れの強さを次のUpdateまで溜めておく
        //! @param event [in] 被弾イベント
        //-------------------------------------------------------------
        void OnPlayerDamaged(const PlayerDamagedEvent& event);

        //-------------------------------------------------------------
        //! @brief 大技通知のハンドラ。インパクトまでの秒数を次のUpdateまで溜めておく
        //! @param event [in] 大技イベント
        //-------------------------------------------------------------
        void OnPlayerFinisher(const PlayerFinisherEvent& event);

        //! 次のUpdateで揺れへ与えるダメージ量の合計
        //! @note 同じフレームに複数回被弾したら足し合わせる（倍率の上限はUpdateで掛ける）
        float m_pendingShakeDamage = 0.0f;

        //! 次のUpdateで受け付ける大技の、インパクトまでの秒数（-1で要求なし）
        //! @note 同じフレームに複数来たら、先に来るインパクトに合わせる（SlowMotionControllerと同じ）
        float m_pendingZoomImpactDelay = -1.0f;

        std::mt19937                   m_rng{std::random_device{}()};    //!< 揺れ始めの向きに使う乱数生成器
        Tsukino::ECS::ScopedConnection m_damagedConnection;              //!< PlayerDamagedEventの購読
        Tsukino::ECS::ScopedConnection m_finisherConnection;             //!< PlayerFinisherEventの購読
    };
}    // namespace CombatAndroid::ECS
