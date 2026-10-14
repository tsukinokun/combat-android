//-------------------------------------------------------------
//! @file   SoftTargetSystem.cpp
//! @brief  SoftTargetSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Combat/SoftTargetSystem.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/SoftTargetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Utility/Combat/SoftTarget.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kDegToRad = 0.017453292f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void SoftTargetSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        // メニュー・カットシーン中は選び直さない（止まっている間に印が動かないように）
        if(IsGameplayFrozen(registry))
            return;

        const SoftTargetParams& params      = GetSoftTargetParams();
        const float             maxAngle    = std::clamp(params.assistAngleDeg, 0.0f, 180.0f) * kDegToRad;
        const float             selectRange = std::max(params.selectRange, 1.0f);

        auto playerView = registry.View<PlayerComponent, SoftTargetComponent, Tsukino::BuiltIn::ECS::TransformComponent, HealthComponent>();
        for(entt::entity playerEntity : playerView) {
            const auto& player     = playerView.get<PlayerComponent>(playerEntity);
            auto&       softTarget = playerView.get<SoftTargetComponent>(playerEntity);
            const auto& transform  = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity);
            const auto& health     = playerView.get<HealthComponent>(playerEntity);

            softTarget.switchCooldown = std::max(softTarget.switchCooldown - deltaTime, 0.0f);

            if(health.isDead) {
                softTarget.target      = entt::null;
                softTarget.assistReady = false;
                continue;
            }

            const hlslpp::float3 aim = ResolveAimDirection(player, transform.rotation);

            //-------------------------------------------------------------
            // 候補を点数付けする。点数は小さいほど良い（角度のずれと距離をそれぞれ0〜1にして重み付けで足す）。
            // 今のターゲットだけ点数にstickinessを掛けてひいきする
            //-------------------------------------------------------------
            Tsukino::ECS::Entity best           = entt::null;
            float                bestScore      = std::numeric_limits<float>::max();
            bool                 currentIsValid = false;

            auto enemyView = registry.View<EnemyComponent, Tsukino::BuiltIn::ECS::TransformComponent, HealthComponent>();
            for(entt::entity enemyEntity : enemyView) {
                if(enemyView.get<HealthComponent>(enemyEntity).isDead)
                    continue;    // 死亡演出（フェード中）の敵は対象外

                const auto&          enemyTransform = enemyView.get<Tsukino::BuiltIn::ECS::TransformComponent>(enemyEntity);
                const hlslpp::float3 toEnemy(enemyTransform.position.x - transform.position.x, 0.0f,
                                             enemyTransform.position.z - transform.position.z);
                const float          distance = hlslpp::length(toEnemy);
                if(distance > selectRange)
                    continue;

                float angle = 0.0f;
                if(distance > 0.001f) {
                    const float cosAngle = std::clamp(static_cast<float>(hlslpp::dot(toEnemy / distance, aim)), -1.0f, 1.0f);
                    angle                = std::acos(cosAngle);
                }
                if(angle > maxAngle)
                    continue;

                float score = (maxAngle > 0.0f ? angle / maxAngle : 0.0f) * params.angleWeight + (distance / selectRange) * params.distanceWeight;
                if(enemyEntity == softTarget.target) {
                    score *= params.stickiness;
                    currentIsValid = true;
                }
                if(score < bestScore) {
                    bestScore = score;
                    best      = enemyEntity;
                }
            }

            //-------------------------------------------------------------
            // 乗り換え。今のターゲットがまだ候補に入っていて、乗り換えた直後（switchCooldown中）なら据え置く。
            // 候補から外れた（倒れた・範囲外へ出た）ときは待たずに乗り換える
            //-------------------------------------------------------------
            if(currentIsValid && best != softTarget.target && softTarget.switchCooldown > 0.0f) {
                best = softTarget.target;
            }
            if(best != softTarget.target) {
                softTarget.target         = best;
                softTarget.switchCooldown = (best != entt::null) ? params.switchCooldown : 0.0f;
            }

            //-------------------------------------------------------------
            // 振り向きが効く距離か。距離は敵の体の表面までで測る（大きい敵ほど中心が遠くても届くため）
            //-------------------------------------------------------------
            softTarget.assistReady = false;
            if(softTarget.target != entt::null) {
                const auto& targetTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(softTarget.target);
                const auto& enemy           = registry.GetComponent<EnemyComponent>(softTarget.target);
                const hlslpp::float3 toTarget(targetTransform.position.x - transform.position.x, 0.0f,
                                              targetTransform.position.z - transform.position.z);
                const float surfaceDistance = static_cast<float>(hlslpp::length(toTarget)) - enemy.bodyRadius;
                softTarget.assistReady      = surfaceDistance <= params.assistRange;
            }
        }
    }
}    // namespace CombatAndroid::ECS
