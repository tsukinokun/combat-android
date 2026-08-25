//-------------------------------------------------------------
//! @file   PlayerDamageEffectSystem.hpp
//! @brief  PlayerDamageEffectSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/PlayerDamagedEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  PlayerDamageEffectSystem
    //! @brief  PlayerDamagedEventを購読し、被弾が分かる演出（プレイヤーモデルの点滅・
    //!         画面の赤フラッシュ）を毎フレーム進行させるシステム
    //-------------------------------------------------------------
    class PlayerDamageEffectSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief PlayerDamagedEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief 被弾通知のハンドラ
        //! @note  PlayerDamagedEventはCombatSystemのview.eachの内側からPublishされるため、
        //!        ここでコンポーネントを追加・削除するとEnTTのイテレータが壊れる。
        //!        フラグを立てるだけにして、実際の開始処理はUpdateへ一本化する
        //-------------------------------------------------------------
        void OnPlayerDamaged(const PlayerDamagedEvent& event);

        bool                             m_pendingTrigger = false;    //!< 次のUpdateで演出を開始すべきか
        Tsukino::ECS::ScopedConnection m_damagedConnection;           //!< PlayerDamagedEventの購読
    };
}    // namespace CombatAndroid::ECS
