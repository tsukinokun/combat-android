//-------------------------------------------------------------
//! @file   EnemyWeaponDropSystem.hpp
//! @brief  EnemyWeaponDropSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/EnemyDiedEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyWeaponDropSystem
    //! @brief  EnemyDiedEventを購読し、敵が手に持っていた武器を死亡位置の地面へ落とすシステム。
    //!         落とした武器はPickupComponentを持つ「拾える武器」になり、以降は
    //!         手置きの武器とまったく同じくPickupSystemがFキーでの取得を処理する
    //! @note   武器は敵の生成時（SpawnBehaviorEnemy）に作られた実体をそのまま使い回す。
    //!         作り直さないので、WeaponSpawner::ConfigureWeaponが焼き込んだ性能
    //!         （専用攻撃モーション・AoE・溜め攻撃）がそのまま引き継がれる
    //-------------------------------------------------------------
    class EnemyWeaponDropSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief EnemyDiedEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除はm_diedConnectionのデストラクタに任せてよい（ExpOrbSystemと同じ作法）
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief 死亡通知のハンドラ
        //! @note  EnemyDiedEventはビヘイビアツリーのアクション（View反復中）から
        //!        Publishされるため、ここでPickupComponentを足すとEnTTのイテレータが
        //!        壊れる。キューへ積むだけにして、ECSの変更はUpdateへ一本化する
        //-------------------------------------------------------------
        void OnEnemyDied(const EnemyDiedEvent& event);

        std::vector<EnemyDiedEvent>    m_pending;           //!< 次のUpdateで処理する死亡通知（武器を持っていたものだけ）
        Tsukino::ECS::ScopedConnection m_diedConnection;    //!< EnemyDiedEventの購読
    };
}    // namespace CombatAndroid::ECS
