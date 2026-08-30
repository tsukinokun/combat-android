//-------------------------------------------------------------
//! @file   EnemyWeaponDropSystem.cpp
//! @brief  EnemyWeaponDropSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/EnemyWeaponDropSystem.hpp>

#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 落とした武器を置く高さ。シーンへ手置きしている武器と同じ値にして、
        //! 拾えるアイテムが地面から浮く高さを揃える
        constexpr float kDropGroundHeight = 10.0f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief EnemyDiedEventの購読を開始する
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_diedConnection = eventBus.Subscribe<EnemyDiedEvent>([this](const EnemyDiedEvent& event) { OnEnemyDied(event); });
    }

    //-------------------------------------------------------------
    //! @brief 死亡通知のハンドラ
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::OnEnemyDied(const EnemyDiedEvent& event) {
        // 武器を持っていない敵（ゾンビ系）の通知は積まない。
        // 撃破の大半はこちらなので、Update側の空回りを避ける
        if(event.heldWeaponEntity == entt::null)
            return;

        m_pending.push_back(event);
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        if(m_pending.empty())
            return;

        for(const EnemyDiedEvent& pending : m_pending) {
            // 死亡位置は敵の足元。武器は接地高さへ置き直して横たわらせる
            hlslpp::float3 dropPosition = pending.position;
            dropPosition.y              = kDropGroundHeight;

            DropWeaponToWorld(registry, pending.heldWeaponEntity, dropPosition);
        }

        m_pending.clear();
    }
}    // namespace CombatAndroid::ECS
