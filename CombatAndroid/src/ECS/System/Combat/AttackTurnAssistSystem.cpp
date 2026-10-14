//-------------------------------------------------------------
//! @file   AttackTurnAssistSystem.cpp
//! @brief  AttackTurnAssistSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Combat/AttackTurnAssistSystem.hpp>
#include <CombatAndroid/ECS/Component/Combat/SoftTargetComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Utility/Combat/SoftTarget.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Core/Math/MathHelper.hpp>

#include <entt/entt.hpp>

#include <algorithm>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void AttackTurnAssistSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        if(IsGameplayFrozen(registry))
            return;

        const float duration = GetSoftTargetParams().turnDuration;

        auto view = registry.View<PlayerComponent, SoftTargetComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(entt::entity entity : view) {
            const auto& player     = view.get<PlayerComponent>(entity);
            auto&       softTarget = view.get<SoftTargetComponent>(entity);
            auto&       transform  = view.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);

            if(!softTarget.turning)
                continue;

            // 回避に入ったら打ち切る（回避の向きはPlayerAnimationSystemが開始時に決めている）
            if(player.isDodging) {
                softTarget.turning = false;
                continue;
            }

            softTarget.turnElapsed += deltaTime;
            const float t = (duration > 0.0f) ? std::min(softTarget.turnElapsed / duration, 1.0f) : 1.0f;

            //-------------------------------------------------------------
            // 少しだけ先が速く後がゆっくりな補間（ease-out）。最初のフレームから大きく向きが変わるので、
            // 振り始めと同時に出る当たり判定にも間に合いやすい。
            // PlayerSystemと同じく、slerpの結果は必ず正規化してから書く（Jolt・武器の追従がrotationを読むため）
            //-------------------------------------------------------------
            const float eased  = 1.0f - (1.0f - t) * (1.0f - t);
            transform.rotation = hlslpp::normalize(Tsukino::Core::Math::SlerpShortestPath(softTarget.turnFrom, softTarget.turnTo, eased));
            transform.dirty    = true;

            if(t >= 1.0f)
                softTarget.turning = false;
        }
    }
}    // namespace CombatAndroid::ECS
