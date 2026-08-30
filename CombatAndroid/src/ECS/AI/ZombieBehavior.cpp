//-------------------------------------------------------------
//! @file   ZombieBehavior.cpp
//! @brief  ゾンビ系敵（SmallZombie/BigZombie共通）用ビヘイビアツリー構築関数の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/AI/ZombieBehavior.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyHeldWeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Event/EnemyDiedEvent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>

#include <hlsl++.h>
#include <algorithm>
#include <cmath>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @brief  "Death"分岐の条件。死亡しているか
        //-------------------------------------------------------------
        bool IsDead(BehaviorContext<EnemyBlackboard>& context) {
            const auto* health = context.registry.try_get<HealthComponent>(context.entity);
            return health && health->isDead;
        }

        //-------------------------------------------------------------
        //! @brief  "Knockback"分岐の条件。ノックバック開始待ち、または硬直中か
        //-------------------------------------------------------------
        bool ShouldKnockback(BehaviorContext<EnemyBlackboard>& context) {
            const auto& enemy = context.registry.GetComponent<EnemyComponent>(context.entity);
            return enemy.pendingKnockback || enemy.isKnockedBack;
        }

        //-------------------------------------------------------------
        //! @brief  "Attack"分岐の条件。攻撃射程内 かつ クールタイム明けか
        //-------------------------------------------------------------
        bool CanAttack(BehaviorContext<EnemyBlackboard>& context) {
            if(!context.blackboard.hasPlayer)
                return false;

            const auto& enemy = context.registry.GetComponent<EnemyComponent>(context.entity);
            return context.blackboard.distanceToPlayer <= enemy.attackRange && enemy.attackCooldownTimer <= 0.0f;
        }

        //-------------------------------------------------------------
        //! @brief  "Chase"分岐の条件。索敵範囲内か
        //-------------------------------------------------------------
        bool CanChase(BehaviorContext<EnemyBlackboard>& context) {
            if(!context.blackboard.hasPlayer)
                return false;

            const auto& enemy = context.registry.GetComponent<EnemyComponent>(context.entity);
            return context.blackboard.distanceToPlayer <= enemy.detectRange;
        }

        //-------------------------------------------------------------
        //! @brief  死亡アクション。Stunnedを1回再生しきってからフェードアウトし、消滅させる
        //! @details
        //! 常にRunningを返す（Death分岐から出ることは無い）。
        //! Stunnedの再生完了はAnimationPlayerComponent::is_finishedで判定し、
        //! 万一クリップ設定ミス等でis_finishedが立たなくてもdeathTimeoutSafetyで
        //! フェードへ進めるようにしてある（PlayAttackと同じ考え方）
        //-------------------------------------------------------------
        NodeStatus PlayDeath(BehaviorContext<EnemyBlackboard>& context) {
            auto& enemy   = context.registry.GetComponent<EnemyComponent>(context.entity);
            auto& animSet = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);

            animSet.desiredState = EnemyAnimState::Death;

            if(!enemy.isDeathAnimFinished) {
                animSet.deathTimer += context.deltaTime;

                bool isPlayingDeathClip = animSet.currentState == EnemyAnimState::Death;
                bool clipFinished        = false;
                if(isPlayingDeathClip) {
                    const auto& animPlayer = context.registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(context.entity);
                    clipFinished             = animPlayer.is_finished;
                }

                bool timedOut = animSet.deathTimer >= animSet.deathTimeoutSafety;

                if((isPlayingDeathClip && clipFinished) || timedOut) {
                    enemy.isDeathAnimFinished = true;

                    // HPバーは死亡と同時に隠す（フェード中に残るのを防ぐ）
                    if(auto* health = context.registry.try_get<HealthComponent>(context.entity))
                        health->hpBarVisibleTimer = 0.0f;
                }

                return NodeStatus::Running;
            }

            //-------------------------------------------------------------
            // Stunned再生完了後：フェードアウトして消滅させる
            //-------------------------------------------------------------
            enemy.deathFadeTimer += context.deltaTime;

            float fadeProgress = enemy.deathFadeDuration > 0.0f ? enemy.deathFadeTimer / enemy.deathFadeDuration : 1.0f;
            float opacity        = std::clamp(1.0f - fadeProgress, 0.0f, 1.0f);

            if(auto* model = context.registry.try_get<Tsukino::BuiltIn::ECS::ModelComponent>(context.entity))
                model->opacity = opacity;

            if(fadeProgress >= 1.0f) {
                // 消滅の直前にEXP玉ドロップ等の副作用処理へ通知する。
                // ここでregistry.CreateEntity()を直接呼ぶとBT反復中のView/Poolを
                // 壊しかねないため、WeaponHitEventと同じくイベント経由にし、
                // 実際のエンティティ生成はExpOrbSystem::Updateへ一本化する
                if(auto* eventBus = context.registry.GetContext<Tsukino::ECS::EventBus*>()) {
                    const auto& deathTransform = context.registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(context.entity);

                    // 手に武器を持っていれば（Paladin等）、それも一緒に通知して地面へ落とさせる。
                    // ここで武器エンティティのコンポーネント構成を変えるとBT反復中のView/Poolを
                    // 壊しかねないため、実際のドロップ処理はEnemyWeaponDropSystem::Updateへ委ねる
                    Tsukino::ECS::Entity heldWeaponEntity = entt::null;
                    if(auto* heldWeapon = context.registry.try_get<EnemyHeldWeaponComponent>(context.entity))
                        heldWeaponEntity = heldWeapon->weaponEntity;

                    eventBus->Publish(
                        EnemyDiedEvent{deathTransform.position, static_cast<int>(enemy.expReward + 0.5f), heldWeaponEntity});
                }

                // 本体・頭上HPバー（背景・残量）を破棄予約する。
                // View反復中なので即時破棄はしない（CombatSystemの死亡処理と同じ作法）
                context.registry.QueueDestroy(context.entity);
                if(auto* health = context.registry.try_get<HealthComponent>(context.entity)) {
                    context.registry.QueueDestroy(health->hpBarBackgroundEntity);
                    context.registry.QueueDestroy(health->hpBarFillEntity);
                }
            }

            return NodeStatus::Running;
        }

        //-------------------------------------------------------------
        //! @brief  ノックバックアクション。desiredStateをKnockbackへ書き、その場で硬直する
        //! @details
        //! 押し戻しは行わない（Transformを一切書かない）。硬直中（isKnockedBack）は
        //! CombatSystem側がpendingKnockbackを立てないため、再ノックバックは発生しない
        //-------------------------------------------------------------
        NodeStatus PlayKnockback(BehaviorContext<EnemyBlackboard>& context) {
            auto& enemy   = context.registry.GetComponent<EnemyComponent>(context.entity);
            auto& animSet = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);

            if(enemy.pendingKnockback) {
                // 立ち上がりの1回だけ：硬直状態へ入る
                enemy.pendingKnockback = false;
                enemy.isKnockedBack     = true;
                animSet.knockbackTimer = 0.0f;
            }

            animSet.desiredState    = EnemyAnimState::Knockback;
            animSet.knockbackTimer += context.deltaTime;

            bool isPlayingKnockbackClip = animSet.currentState == EnemyAnimState::Knockback;
            bool clipFinished             = false;
            if(isPlayingKnockbackClip) {
                const auto& animPlayer = context.registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(context.entity);
                clipFinished             = animPlayer.is_finished;
            }

            bool timedOut = animSet.knockbackTimer >= animSet.knockbackTimeoutSafety;

            if((isPlayingKnockbackClip && clipFinished) || timedOut) {
                enemy.isKnockedBack       = false;
                enemy.attackCooldownTimer = enemy.attackCooldown;    // 怯み明けに即攻撃させない
                return NodeStatus::Success;
            }

            return NodeStatus::Running;    // Transformは一切書かない＝その場で硬直
        }

        //-------------------------------------------------------------
        //! @brief  攻撃アクション。desiredStateをAttackへ書き、再生完了までRunningを返す
        //! @details
        //! 記憶付きSequenceの都合上、Running中はCanAttack（親のCondition）が再評価されない。
        //! そのため終了判定はこの関数自身が持つ：
        //!   - animSet.currentState == Attack かつ AnimationPlayerComponent::is_finished
        //!     （EnemyAnimationSystemが実際にAttackクリップへ切り替え終え、かつ再生が終わった）
        //!   - もしくはattackTimerがattackTimeoutSafetyを超えた（クリップ設定ミス等の保険。
        //!     PlayerAnimationSystemのattackTimeoutSafetyと同じ考え方）
        //-------------------------------------------------------------
        NodeStatus PlayAttack(BehaviorContext<EnemyBlackboard>& context) {
            auto& enemy   = context.registry.GetComponent<EnemyComponent>(context.entity);
            auto& animSet = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);

            animSet.desiredState = EnemyAnimState::Attack;
            animSet.attackTimer += context.deltaTime;

            bool isPlayingAttackClip = animSet.currentState == EnemyAnimState::Attack;
            bool clipFinished        = false;
            if(isPlayingAttackClip) {
                const auto& animPlayer = context.registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(context.entity);
                clipFinished             = animPlayer.is_finished;
            }

            bool timedOut = animSet.attackTimer >= animSet.attackTimeoutSafety;

            if((isPlayingAttackClip && clipFinished) || timedOut) {
                enemy.attackCooldownTimer = enemy.attackCooldown;
                return NodeStatus::Success;
            }

            return NodeStatus::Running;
        }

        //-------------------------------------------------------------
        //! @brief  追跡アクション。desiredStateをWalkへ書き、プレイヤーへ近づく
        //! @details
        //! 単純な直進追跡（Transformを直接書き換える）。
        //! attackRange内に到達したらSuccess（攻撃分岐へ譲る）、detectRange外まで
        //! 逃げられたらFailure（記憶付きSequenceの都合上、この関数自身がCanChase相当の
        //! 再判定を持つ必要がある）を返す
        //-------------------------------------------------------------
        NodeStatus MoveToPlayer(BehaviorContext<EnemyBlackboard>& context) {
            if(!context.blackboard.hasPlayer)
                return NodeStatus::Failure;

            auto& enemy     = context.registry.GetComponent<EnemyComponent>(context.entity);
            auto& transform = context.registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(context.entity);
            auto& animSet   = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);

            hlslpp::float3 toPlayer = context.blackboard.playerPosition - transform.position;
            toPlayer.y              = 0.0f;    // 水平面のみで追跡する

            float distance = hlslpp::length(toPlayer);
            if(distance > enemy.detectRange)
                return NodeStatus::Failure;    // 見失った

            if(distance <= enemy.attackRange) {
                animSet.desiredState = EnemyAnimState::Idle;    // 攻撃分岐へ譲る間はIdleで待機させる（棒立ち防止）
                return NodeStatus::Success;    // 射程内に到達。攻撃分岐へ譲る（このフレームは移動しない）
            }

            if(distance > 0.001f) {
                hlslpp::float3 moveDir = toPlayer / distance;
                transform.position     = transform.position + moveDir * enemy.moveSpeed * context.deltaTime;

                // 移動方向を向かせる
                float yawRad       = std::atan2(moveDir.x, moveDir.z);
                transform.rotation = hlslpp::quaternion::rotation_y(yawRad);
                transform.dirty    = true;
            }

            animSet.desiredState = EnemyAnimState::Walk;
            return NodeStatus::Running;
        }

        //-------------------------------------------------------------
        //! @brief  待機アクション。desiredStateをIdleへ書く
        //! @details
        //! 必ずSuccessを返すこと。Runningを返すと記憶付きSelectorがこのIdle分岐に
        //! 貼り付いたままになり、上位のAttack/Chase分岐が二度と評価されなくなる
        //-------------------------------------------------------------
        NodeStatus PlayIdle(BehaviorContext<EnemyBlackboard>& context) {
            auto& animSet         = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);
            animSet.desiredState = EnemyAnimState::Idle;
            return NodeStatus::Success;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief  ゾンビ系敵用のビヘイビアツリーを構築する
    //-------------------------------------------------------------
    std::shared_ptr<EnemyBehaviorNode> BuildZombieTree() {
        auto deathSequence = std::make_shared<Sequence<EnemyBlackboard>>();
        deathSequence->AddChild(std::make_shared<ConditionNode<EnemyBlackboard>>(&IsDead));
        deathSequence->AddChild(std::make_shared<ActionNode<EnemyBlackboard>>(&PlayDeath));

        auto knockbackSequence = std::make_shared<Sequence<EnemyBlackboard>>();
        knockbackSequence->AddChild(std::make_shared<ConditionNode<EnemyBlackboard>>(&ShouldKnockback));
        knockbackSequence->AddChild(std::make_shared<ActionNode<EnemyBlackboard>>(&PlayKnockback));

        auto attackSequence = std::make_shared<Sequence<EnemyBlackboard>>();
        attackSequence->AddChild(std::make_shared<ConditionNode<EnemyBlackboard>>(&CanAttack));
        attackSequence->AddChild(std::make_shared<ActionNode<EnemyBlackboard>>(&PlayAttack));

        auto chaseSequence = std::make_shared<Sequence<EnemyBlackboard>>();
        chaseSequence->AddChild(std::make_shared<ConditionNode<EnemyBlackboard>>(&CanChase));
        chaseSequence->AddChild(std::make_shared<ActionNode<EnemyBlackboard>>(&MoveToPlayer));

        auto root = std::make_shared<Selector<EnemyBlackboard>>();
        root->AddChild(deathSequence);
        root->AddChild(knockbackSequence);
        root->AddChild(attackSequence);
        root->AddChild(chaseSequence);
        root->AddChild(std::make_shared<ActionNode<EnemyBlackboard>>(&PlayIdle));

        return root;
    }
}    // namespace CombatAndroid::ECS
