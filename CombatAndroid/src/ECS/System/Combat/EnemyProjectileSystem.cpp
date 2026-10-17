//-------------------------------------------------------------
//! @file   EnemyProjectileSystem.cpp
//! @brief  EnemyProjectileSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Combat/EnemyProjectileSystem.hpp>
#include <CombatAndroid/ECS/Component/Combat/EnemyProjectileComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Utility/Combat/CombatHit.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/Core/Log.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 弾が当たったときのヒットストップ（体の判定より短い。小さな弾なので軽く）
        constexpr float kWebHitStopDuration = 0.08f;
        constexpr float kWebHitStopScale    = 0.3f;

        //-------------------------------------------------------------
        //! @brief  点と線分の距離の2乗
        //-------------------------------------------------------------
        float DistanceSqPointToSegment(const hlslpp::float3& point, const hlslpp::float3& a, const hlslpp::float3& b) {
            const hlslpp::float3 ab     = b - a;
            const float          lenSq  = hlslpp::dot(ab, ab);
            const float          t      = lenSq > 1e-6f ? std::clamp(static_cast<float>(hlslpp::dot(point - a, ab)) / lenSq, 0.0f, 1.0f) : 0.0f;
            const hlslpp::float3 offset = point - (a + ab * t);
            return hlslpp::dot(offset, offset);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyProjectileSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        //-------------------------------------------------------------
        // プレイヤーのカプセルの芯線（CombatSystemと同じく、CharacterControllerの寸法から直接組み立てる）
        //-------------------------------------------------------------
        Tsukino::ECS::Entity playerEntity = entt::null;
        hlslpp::float3       capsuleTop(0.0f, 0.0f, 0.0f);
        hlslpp::float3       capsuleBottom(0.0f, 0.0f, 0.0f);
        float                capsuleRadius = 0.0f;

        auto playerView = registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::CharacterControllerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(entt::entity entity : playerView) {
            const auto&          controller = playerView.get<Tsukino::BuiltIn::ECS::CharacterControllerComponent>(entity);
            const auto&          transform  = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            const hlslpp::float3 center     = transform.position + hlslpp::mul(controller.centerOffset, transform.rotation);
            capsuleTop                      = center + hlslpp::float3(0.0f, controller.halfHeight, 0.0f);
            capsuleBottom                   = center - hlslpp::float3(0.0f, controller.halfHeight, 0.0f);
            capsuleRadius                   = controller.radius;
            playerEntity                    = entity;
            break;
        }

        std::vector<Tsukino::ECS::Entity> finished;

        auto view = registry.View<EnemyProjectileComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](entt::entity entity, EnemyProjectileComponent& projectile, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            const float step = projectile.speed * deltaTime;
            transform.position = transform.position + projectile.direction * step;
            transform.dirty    = true;
            projectile.remaining -= step;

            if(playerEntity != entt::null) {
                const float reach = projectile.radius + capsuleRadius;
                if(DistanceSqPointToSegment(transform.position, capsuleBottom, capsuleTop) <= reach * reach) {
                    // 無敵（回避中など）ならすり抜けてそのまま飛び続ける
                    if(DamagePlayer(registry, playerEntity, projectile.owner, projectile.damage, transform.position, kWebHitStopDuration, kWebHitStopScale)) {
                        if(projectile.slowSeconds > 0.0f) {
                            auto& player     = registry.GetComponent<PlayerComponent>(playerEntity);
                            player.slowTimer = std::max(player.slowTimer, projectile.slowSeconds);
                            player.slowScale = projectile.slowScale;
#ifdef _DEBUG
                            Tsukino::Core::Log::Info("EnemyProjectile: slowed player for " + std::to_string(projectile.slowSeconds) + "s");
#endif
                        }
                        finished.push_back(entity);
                        return;
                    }
                }
            }

            if(projectile.remaining <= 0.0f)
                finished.push_back(entity);
        });

        // 反復の外で破棄する（System内からの破棄はQueueDestroy）
        for(Tsukino::ECS::Entity entity : finished)
            registry.QueueDestroy(entity);
    }
}    // namespace CombatAndroid::ECS
