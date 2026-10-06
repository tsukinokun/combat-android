//-------------------------------------------------------------
//! @file   FogFollowSystem.cpp
//! @brief  FogFollowSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/World/FogFollowSystem.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FogComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void FogFollowSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        //-------------------------------------------------------------
        // プレイヤー位置の取得。CharacterControllerComponentを持つ最初の
        // エンティティを追う（GroundFollowSystemのプレイヤー位置取得と同じ流儀）
        //-------------------------------------------------------------
        hlslpp::float3 playerPosition(0.0f, 0.0f, 0.0f);
        bool           foundPlayer = false;

        auto playerView =
            registry.View<Tsukino::BuiltIn::ECS::CharacterControllerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        playerView.each([&](entt::entity, const Tsukino::BuiltIn::ECS::CharacterControllerComponent&,
                            const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            if(foundPlayer)
                return;

            playerPosition = transform.position;
            foundPlayer    = true;
        });

        if(!foundPlayer)
            return;

        //-------------------------------------------------------------
        // フォグの距離フォグ基準点をプレイヤー位置へ切り替える
        //-------------------------------------------------------------
        auto fogView = registry.View<Tsukino::BuiltIn::ECS::FogComponent>();
        fogView.each([&](entt::entity, Tsukino::BuiltIn::ECS::FogComponent& fog) {
            fog.useCustomDistanceOrigin = true;
            fog.distanceOrigin          = playerPosition;
        });
    }

}    // namespace CombatAndroid::ECS
