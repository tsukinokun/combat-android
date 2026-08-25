//-------------------------------------------------------------
//! @file   EnemyBehaviorSystem.cpp
//! @brief  EnemyBehaviorSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/EnemyBehaviorSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Component/BehaviorTreeComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyBehaviorSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        //-------------------------------------------------------------
        // スキル選択メニュー中は敵の思考ごと止める。BTの移動アクションが
        // CharacterControllerComponent::moveInputを書くため、放っておくと
        // SkillSelectSystemが打ち消した値が毎フレーム戻されてしまう
        //-------------------------------------------------------------
        if(IsSkillSelectActive(registry))
            return;

        //-------------------------------------------------------------
        // プレイヤーの位置を取得（単一プレイヤー前提。EnemySystemと同じ方針）
        //-------------------------------------------------------------
        entt::entity   playerEntity   = entt::null;
        hlslpp::float3 playerPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);

        auto playerView = registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(auto entity : playerView) {
            playerEntity   = entity;
            playerPosition = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
            break;
        }

        //-------------------------------------------------------------
        // ビヘイビアツリーを持つ敵ごとに黒板を更新し、ツリーを1回進行させる
        //-------------------------------------------------------------
        auto view = registry.View<BehaviorTreeComponent, EnemyComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](entt::entity                                entity,
                     BehaviorTreeComponent&                      behaviorTree,
                     EnemyComponent&                               enemy,
                     Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            // 死亡・ノックバックはRunning中のAttack/Chase分岐を即座に打ち切りたいが、
            // 記憶付きSequence/Selectorは自分より前段の条件をRunning中は再評価しない
            // （BehaviorTree.hpp参照）。そこでこの2つのケースだけは明示的にReset()して
            // 次のTickを必ず先頭（Death/Knockback分岐）から評価させる。
            // まだDeathへ移っていない場合に限定することで、死亡後は毎フレームResetし続けない
            auto* health = registry.try_get<HealthComponent>(entity);
            auto* animSet = registry.try_get<EnemyAnimationSetComponent>(entity);
            bool  justDied = health && health->isDead && animSet && animSet->desiredState != EnemyAnimState::Death;
            if(enemy.pendingKnockback || justDied) {
                if(behaviorTree.root)
                    behaviorTree.root->Reset();
            }

            // 攻撃クールタイムの減算（死亡・硬直中でも進めて構わない。硬直明けにPlayKnockbackが
            // 上書きするため、ここで負にしても実害はない）
            if(enemy.attackCooldownTimer > 0.0f) {
                enemy.attackCooldownTimer -= deltaTime;
                if(enemy.attackCooldownTimer < 0.0f)
                    enemy.attackCooldownTimer = 0.0f;
            }

            // 黒板の更新
            EnemyBlackboard& blackboard = behaviorTree.blackboard;
            blackboard.playerEntity      = playerEntity;
            blackboard.hasPlayer         = playerEntity != entt::null;

            if(blackboard.hasPlayer) {
                blackboard.playerPosition = playerPosition;

                hlslpp::float3 toPlayer = playerPosition - transform.position;
                toPlayer.y              = 0.0f;    // 水平面のみで判定する
                blackboard.distanceToPlayer = hlslpp::length(toPlayer);
            } else {
                blackboard.distanceToPlayer = 0.0f;
            }

            // ツリーを1回進行させる
            if(behaviorTree.root) {
                BehaviorContext<EnemyBlackboard> context{registry, entity, blackboard, deltaTime};
                behaviorTree.root->Tick(context);
            }
        });
    }
}    // namespace CombatAndroid::ECS
