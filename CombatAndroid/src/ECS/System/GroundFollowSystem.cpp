//-------------------------------------------------------------
//! @file   GroundFollowSystem.cpp
//! @brief  GroundFollowSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/GroundFollowSystem.hpp>
#include <CombatAndroid/ECS/Component/GroundFollowComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void GroundFollowSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        //-------------------------------------------------------------
        // プレイヤー位置の取得。CharacterControllerComponentを持つ最初の
        // エンティティを追う（GrassFieldSystemのプレイヤー位置取得と同じ流儀）
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
        // 地面（GroundFollowComponentを持つエンティティ）を追従させる。
        // 位置(x/z)だけプレイヤーへ合わせ、Yは常にgroundHeightで固定する
        //-------------------------------------------------------------
        auto groundView = registry.View<GroundFollowComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        groundView.each([&](entt::entity, const GroundFollowComponent& groundFollow, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            transform.position = hlslpp::float3(playerPosition.x, groundFollow.groundHeight, playerPosition.z);
            transform.dirty    = true;
        });
    }

}    // namespace CombatAndroid::ECS
