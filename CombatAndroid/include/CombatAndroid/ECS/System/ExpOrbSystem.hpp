//-------------------------------------------------------------
//! @file   ExpOrbSystem.hpp
//! @brief  ExpOrbSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/EnemyDiedEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <hlsl++.h>

#include <random>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  ExpOrbSystem
    //! @brief  EnemyDiedEventを購読し、死亡位置からEXP玉をポップさせるシステム。
    //!         玉は「重力で落下・着地」→「プレイヤーへ加速しながら吸い寄せられる」→
    //!         「吸収してEXPを加算」の順に演出し、表示にはシーン生成時に用意した
    //!         kExpOrbPoolSize個のエンティティを使い回す（実行時の生成・破棄は行わない）
    //-------------------------------------------------------------
    class ExpOrbSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief EnemyDiedEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除はm_diedConnectionのデストラクタに任せてよい（DamageNumberSystemと同じ作法）
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief 死亡通知のハンドラ
        //! @note  EnemyDiedEventはビヘイビアツリーのアクション（View反復中）から
        //!        Publishされるため、ここでコンポーネントを追加・削除するとEnTTの
        //!        イテレータが壊れる。キューへ積むだけにして、ECSの変更はUpdateへ一本化する
        //-------------------------------------------------------------
        void OnEnemyDied(const EnemyDiedEvent& event);

        std::vector<EnemyDiedEvent>    m_pending;           //!< 次のUpdateで処理する死亡通知
        Tsukino::ECS::ScopedConnection m_diedConnection;    //!< EnemyDiedEventの購読
        std::mt19937                   m_rng{std::random_device{}()};    //!< 落下時の散らばり方向に使う乱数生成器
    };
}    // namespace CombatAndroid::ECS
