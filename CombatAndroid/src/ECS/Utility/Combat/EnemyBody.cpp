//-------------------------------------------------------------
//! @file   EnemyBody.cpp
//! @brief  敵の体の中心を引く共通処理の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Combat/EnemyBody.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief 敵の体の中心を得る
    //-------------------------------------------------------------
    hlslpp::float3 GetEnemyBodyCenter(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        const auto& transform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
        const auto* collision = registry.try_get<Tsukino::BuiltIn::ECS::CollisionComponent>(entity);
        if(!collision)
            return transform.position;
        // PhysicsSystemがボディを置くのと同じ式（オフセットは向きで回し、スケールは掛けない）
        return transform.position + hlslpp::mul(collision->offsetPosition, transform.rotation);
    }
}    // namespace CombatAndroid::ECS
