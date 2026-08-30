//-------------------------------------------------------------
//! @file   GameLogSystem.hpp
//! @brief  GameLogSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/GameLogEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  GameLogSystem
    //! @brief  GameLogEventを購読し、画面右側へ「武器を取得」「レベルアップ」等の
    //!         取得ログを積み上げるシステム。
    //!         新しい行は常に一番下の段へ右からスライドインし、古い行は1段ずつ
    //!         上へ押し上げられて、寿命が尽きると上へふわっとフェードして消える。
    //!         表示にはシーン生成時に用意したkGameLogPoolSize個のスロット
    //!         （1スロット4エンティティ）を使い回し、実行時のエンティティ生成・破棄は
    //!         一切行わない
    //-------------------------------------------------------------
    class GameLogSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief GameLogEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除はm_logConnectionのデストラクタに任せてよい（OnExitで行うことは無い）
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief 取得ログ通知のハンドラ
        //! @note  GameLogEventはExpOrbSystemのview.eachの内側からもPublishされるため、
        //!        ここでコンポーネントを追加・削除するとEnTTのイテレータが壊れる。
        //!        キューへ積むだけにして、ECSの変更はUpdateへ一本化する
        //!        （DamageNumberSystem::OnWeaponHitと同じ流儀）
        //-------------------------------------------------------------
        void OnGameLog(const GameLogEvent& event);

        std::vector<GameLogEvent>      m_pending;             //!< 次のUpdateで処理するログ
        Tsukino::ECS::ScopedConnection m_logConnection;       //!< GameLogEventの購読
        unsigned int                   m_nextSequence = 0;    //!< 発生順の採番カウンタ
    };
}    // namespace CombatAndroid::ECS
