//-------------------------------------------------------------
//! @file   BossBehavior.cpp
//! @brief  ボス用ビヘイビアツリー構築関数の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/AI/BossBehavior.hpp>
#include <CombatAndroid/ECS/AI/ZombieBehavior.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BossMoveSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Event/Enemy/BossProjectileEvent.hpp>
#include <CombatAndroid/ECS/Event/Enemy/BossSummonEvent.hpp>
#include <CombatAndroid/ECS/System/World/NightTransitionSystem.hpp>
#include <CombatAndroid/ECS/Utility/Table/BossMoveTable.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Log.hpp>

#include <hlsl++.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 技の抽選に使う乱数（ボスは同時に1体なので共有でよい）
        std::mt19937& BossRng() {
            static std::mt19937 s_rng{std::random_device{}()};
            return s_rng;
        }

        //! 技がクリップの終わりを待たずに抜けられなくなった場合の保険（秒）。技の長さにこれを足して打ち切る
        constexpr float kMoveTimeoutMargin = 2.0f;

        //! 激昂中の連続攻撃で、1回目と2回目の間に挟む短い隙（秒）。
        //! Attackステートを一度抜けないと同じクリップを頭から再生し直せないため
        constexpr float kRepeatGap = 0.12f;

        //-------------------------------------------------------------
        //! @brief  モデルの正面（+Z）の水平の向き（ラジアン）を得る
        //-------------------------------------------------------------
        float GetYaw(const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            const hlslpp::float3 forward = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), transform.rotation);
            return std::atan2(static_cast<float>(forward.x), static_cast<float>(forward.z));
        }

        //-------------------------------------------------------------
        //! @brief  狙う点の方へ向き直る
        //! @param  maxStep [in] このフレームで回してよい角度（ラジアン）。負なら一気に向く
        //-------------------------------------------------------------
        void TurnToward(Tsukino::BuiltIn::ECS::TransformComponent& transform, const hlslpp::float3& target, float maxStep) {
            const float dx = static_cast<float>(target.x) - static_cast<float>(transform.position.x);
            const float dz = static_cast<float>(target.z) - static_cast<float>(transform.position.z);
            if(dx * dx + dz * dz < 1.0f)
                return;

            const float desired = std::atan2(dx, dz);
            float       yaw     = desired;
            if(maxStep >= 0.0f) {
                const float current = GetYaw(transform);
                float       diff    = std::remainder(desired - current, 6.2831853f);
                diff                = std::clamp(diff, -maxStep, maxStep);
                yaw                 = current + diff;
            }

            transform.rotation = hlslpp::quaternion::rotation_y(yaw);
            transform.dirty    = true;
        }

        //-------------------------------------------------------------
        //! @brief  技を始める。クリップと攻撃判定を技の値へ書き換え、Attackステートへ入る
        //! @param  fresh [in] 新しく選んだ技か（false＝激昂中の連続攻撃の2回目以降）
        //-------------------------------------------------------------
        void StartMove(BehaviorContext<EnemyBlackboard>& context, BossMoveSetComponent& boss, const BossMoveSet& set, int moveIndex, bool fresh) {
            const BossMoveDef& move = set.moves[static_cast<size_t>(moveIndex)];

            auto& animSet   = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);
            auto& hitbox    = context.registry.GetComponent<EnemyAttackHitboxComponent>(context.entity);
            auto& transform = context.registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(context.entity);

            boss.currentMove = moveIndex;
            boss.phase       = BossActionPhase::Move;
            boss.phaseTimer  = 0.0f;
            boss.summoned     = false;
            boss.chargeLocked = false;
            if(fresh)
                boss.repeatLeft = boss.enraged ? std::max(move.enragedRepeat - 1, 0) : 0;

            //-------------------------------------------------------------
            // クリップはAttackステートへ入る瞬間にEnemyAnimationSystemが読むので、ここで差し替えておけばよい
            //-------------------------------------------------------------
            if(static_cast<size_t>(moveIndex) < boss.clips.size() && boss.clips[static_cast<size_t>(moveIndex)].IsValid())
                animSet.attackClip = boss.clips[static_cast<size_t>(moveIndex)];
            animSet.attackIndex   = move.animationIndex;
            animSet.attackLooping = move.loopClip;
            animSet.attackClipEnd = 0.0f;    // ダウンで途中止めにした設定を戻す

            //-------------------------------------------------------------
            // 判定を技の値へ。ボーンを替えたら名前の解決をやり直させる（CombatSystemのキャッシュを無効にする）
            //-------------------------------------------------------------
            hitbox.boneName                = move.hitbox.bone;
            hitbox.endBoneName             = move.hitbox.endBone;
            hitbox.boneNodeIndex           = UINT32_MAX;
            hitbox.endBoneNodeIndex        = UINT32_MAX;
            hitbox.resolvedAgainstModel    = Tsukino::Asset::AssetHandle::Invalid();
            hitbox.resolvedAgainstModelEnd = Tsukino::Asset::AssetHandle::Invalid();
            hitbox.boneLocalOffset         = hlslpp::float3(0.0f, 0.0f, 0.0f);
            hitbox.endBoneLocalOffset      = hlslpp::float3(0.0f, 0.0f, 0.0f);
            hitbox.radius                  = move.hitbox.radius * boss.sizeScale;
            hitbox.damage                  = move.hitbox.damage * boss.damageScale;
            hitbox.hitStartTime            = move.hitbox.start;
            hitbox.hitDuration             = move.hitbox.duration;

            //-------------------------------------------------------------
            // 突進は駆けている間ずっと体に判定を出す。予兆（赤い光・足元の帯）は駆け出す時刻までになる。
            // ダメージ0の技（弾・召喚）は判定を出さない（長さ0にすると判定区間に入らない）
            //-------------------------------------------------------------
            if(move.kind == BossMoveKind::Charge) {
                hitbox.hitStartTime = move.chargeStart;
                hitbox.hitDuration  = move.chargeDuration;
            }
            if(move.hitbox.damage <= 0.0f)
                hitbox.hitDuration = 0.0f;

            //-------------------------------------------------------------
            // 狙い：跳びかかりは今のプレイヤーの位置を着地点にする（跳べる距離で切る）。
            // 予兆の円をここに出すので、決めた後は動かさない（見てから避けられるように）
            //-------------------------------------------------------------
            boss.moveOrigin = transform.position;
            boss.moveTarget = context.blackboard.playerPosition;
            boss.moveTarget.y = transform.position.y;
            if(move.leapStart >= 0.0f) {
                hlslpp::float3 toTarget = boss.moveTarget - boss.moveOrigin;
                toTarget.y              = 0.0f;
                const float distance    = hlslpp::length(toTarget);
                const float maxDistance = move.leapMaxDistance * boss.sizeScale;
                //-------------------------------------------------------------
                // 着地点はプレイヤーの手前（判定の半径の半分）に置く。真上に降りると体が重なったまま
                // 次の技へ移ってしまう。半分なら、立ち止まっているプレイヤーには当たり、動けば避けられる
                //-------------------------------------------------------------
                const float landOffset = move.hitbox.radius * boss.sizeScale * 0.5f;
                const float landing    = std::clamp(distance - landOffset, 0.0f, maxDistance);
                if(distance > 0.001f)
                    boss.moveTarget = boss.moveOrigin + toTarget * (landing / distance);
            }

            //-------------------------------------------------------------
            // 旋回は攻撃しないので、Attackへ入らず歩きのまま回る。回る向きは毎回くじで決める
            //-------------------------------------------------------------
            if(move.kind == BossMoveKind::Orbit) {
                boss.orbitSign       = std::uniform_int_distribution<int>(0, 1)(BossRng()) == 0 ? -1.0f : 1.0f;
                animSet.desiredState = EnemyAnimState::Walk;
            } else {
                animSet.desiredState = EnemyAnimState::Attack;
            }

#ifdef _DEBUG
            // 技の選ばれ方を調整するときに、どの技がどの距離で出たかを Logs/Tsukino.log で追えるようにする
            Tsukino::Core::Log::Info("Boss[" + boss.key + "] move=" + move.name + " distance=" + std::to_string(static_cast<int>(context.blackboard.distanceToPlayer))
                                     + (boss.enraged ? " enraged" : "") + (fresh ? "" : " repeat"));
#endif
        }

        //-------------------------------------------------------------
        //! @brief  ダウンさせる。技を打ち切り、ダウンのクリップを途中まで再生して止め、攻撃判定と予兆を消す
        //-------------------------------------------------------------
        void StartBreak(BehaviorContext<EnemyBlackboard>& context, BossMoveSetComponent& boss, const BossMoveSet& set) {
            auto& animSet = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);
            auto& hitbox  = context.registry.GetComponent<EnemyAttackHitboxComponent>(context.entity);

            boss.phase        = BossActionPhase::Broken;
            boss.phaseTimer   = 0.0f;
            boss.currentMove  = -1;
            boss.repeatLeft   = 0;
            boss.breakPending = false;
            ++boss.breakCount;

            // 判定は出さない（予兆の光も判定の時刻に向けて強まるので、時刻を遠くへ置いて消す）
            hitbox.hitDuration  = 0.0f;
            hitbox.hitStartTime = 1.0e6f;

            //-------------------------------------------------------------
            // 攻撃ステートの仕組みで「ダウンのクリップを途中まで再生して止める」。
            // 同じ攻撃ステートのままだとクリップが切り替わらないので、一度待機を挟む（次のフレームで攻撃へ入る）
            //-------------------------------------------------------------
            animSet.attackClip    = boss.breakClip.IsValid() ? boss.breakClip : animSet.knockbackClip;
            animSet.attackIndex   = set.breakDef.animationIndex;
            animSet.attackLooping = false;
            animSet.attackClipEnd = set.breakDef.clipEnd;
            animSet.desiredState  = EnemyAnimState::Idle;

#ifdef _DEBUG
            Tsukino::Core::Log::Info("Boss[" + boss.key + "] BREAK count=" + std::to_string(boss.breakCount));
#endif
        }

        //-------------------------------------------------------------
        //! @brief  出せる技を距離・クールタイム・重みで1つ選ぶ
        //! @return 技の番号。出せる技が無ければ -1
        //-------------------------------------------------------------
        int PickMove(const BossMoveSetComponent& boss, const BossMoveSet& set, float distance) {
            float total = 0.0f;
            for(size_t i = 0; i < set.moves.size(); ++i) {
                const BossMoveDef& move = set.moves[i];
                if(move.weight > 0.0f && boss.cooldowns[i] <= 0.0f && distance >= move.minRange * boss.sizeScale
                   && distance <= move.maxRange * boss.sizeScale)
                    total += move.weight;
            }
            if(total <= 0.0f)
                return -1;

            float pick = std::uniform_real_distribution<float>(0.0f, total)(BossRng());
            for(size_t i = 0; i < set.moves.size(); ++i) {
                const BossMoveDef& move = set.moves[i];
                if(move.weight > 0.0f && boss.cooldowns[i] <= 0.0f && distance >= move.minRange * boss.sizeScale
                   && distance <= move.maxRange * boss.sizeScale) {
                    pick -= move.weight;
                    if(pick <= 0.0f)
                        return static_cast<int>(i);
                }
            }
            return -1;
        }

        //-------------------------------------------------------------
        //! @brief  ボスの行動。追跡 → 技 → 隙 を状態として持ち、毎フレーム進める
        //! @details
        //! 常にRunningを返す。死亡したときはEnemyBehaviorSystemがツリーをResetし、
        //! 先頭の"Death"分岐から評価し直す
        //-------------------------------------------------------------
        NodeStatus RunBoss(BehaviorContext<EnemyBlackboard>& context) {
            auto* bossPtr = context.registry.try_get<BossMoveSetComponent>(context.entity);
            if(!bossPtr)
                return NodeStatus::Running;

            BossMoveSetComponent& boss      = *bossPtr;
            const BossMoveSet&    set       = GetBossMoveSet(boss.key);
            auto&                 enemy     = context.registry.GetComponent<EnemyComponent>(context.entity);
            auto&                 animSet   = context.registry.GetComponent<EnemyAnimationSetComponent>(context.entity);
            auto&                 transform = context.registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(context.entity);
            const auto&           health    = context.registry.GetComponent<HealthComponent>(context.entity);
            const float           dt        = context.deltaTime;

            if(boss.cooldowns.size() != set.moves.size())
                boss.cooldowns.assign(set.moves.size(), 0.0f);
            for(float& cooldown : boss.cooldowns)
                cooldown = std::max(cooldown - dt, 0.0f);

            boss.phaseTimer += dt;

            //-------------------------------------------------------------
            // 体勢ゲージ：最大値は湧いた後の最大HPから決める（危険度・ボスの倍率込み）。ダウンするたびに増やす。
            // しばらく当てられなければ少しずつ戻る。満タンになったら（ApplyBossHitModifiersが立てる）ダウンする
            //-------------------------------------------------------------
            const BossBreakDef& breakDef = set.breakDef;
            if(breakDef.gaugeRatio > 0.0f) {
                boss.breakGaugeMax = health.maxHealth * breakDef.gaugeRatio * (1.0f + breakDef.growthPerBreak * static_cast<float>(boss.breakCount));
                if(boss.phase != BossActionPhase::Broken) {
                    boss.breakIdleTimer += dt;
                    if(boss.breakIdleTimer >= breakDef.decayDelay)
                        boss.breakGauge = std::max(boss.breakGauge - boss.breakGaugeMax * breakDef.decayRatioPerSecond * dt, 0.0f);
                }
                if(boss.breakPending && boss.phase != BossActionPhase::Broken) {
                    StartBreak(context, boss, set);
                    return NodeStatus::Running;
                }
            }

            //-------------------------------------------------------------
            // 激昂：HPが閾値を切った最初の1回だけ。合図の技は今の技を終えてから出す
            //-------------------------------------------------------------
            if(!boss.enraged && set.enrage.healthRatio > 0.0f && health.maxHealth > 0.0f
               && health.currentHealth / health.maxHealth < set.enrage.healthRatio) {
                boss.enraged           = true;
                boss.enrageMovePending = FindBossMove(set, set.enrage.move) >= 0;
                enemy.moveSpeed        = boss.baseMoveSpeed * set.enrage.moveSpeedScale;

                // 第2形態：夜霧を濃くして視界を狭める（1なら変えない）
                if(set.enrage.fogDensityScale != 1.0f)
                    SetNightFogScale(context.registry, set.enrage.fogDensityScale);
            }
            const float pace = boss.enraged ? set.enrage.cooldownScale : 1.0f;    // 激昂中は技の間隔と隙を縮める

            const float distance = context.blackboard.hasPlayer ? context.blackboard.distanceToPlayer : 1.0e9f;

            switch(boss.phase) {
                //-------------------------------------------------------------
                // 追跡：出せる技があれば出す。無ければ近づく（近すぎるときは止まって向き直る）
                //-------------------------------------------------------------
                case BossActionPhase::Chase: {
                    if(!context.blackboard.hasPlayer) {
                        animSet.desiredState = EnemyAnimState::Idle;
                        break;
                    }

                    if(boss.enrageMovePending) {
                        boss.enrageMovePending = false;
                        StartMove(context, boss, set, FindBossMove(set, set.enrage.move), true);
                        break;
                    }

                    const int moveIndex = PickMove(boss, set, distance);
                    if(moveIndex >= 0) {
                        StartMove(context, boss, set, moveIndex, true);
                        break;
                    }

                    if(distance > set.chaseStopRange * boss.sizeScale) {
                        hlslpp::float3 toPlayer = context.blackboard.playerPosition - transform.position;
                        toPlayer.y              = 0.0f;
                        const float length      = hlslpp::length(toPlayer);
                        if(length > 0.001f)
                            transform.position = transform.position + (toPlayer / length) * enemy.moveSpeed * dt;
                        animSet.desiredState = EnemyAnimState::Walk;
                    } else {
                        animSet.desiredState = EnemyAnimState::Idle;
                    }
                    TurnToward(transform, context.blackboard.playerPosition, set.turnSpeed * 2.0f * dt);
                    break;
                }

                //-------------------------------------------------------------
                // 技：予兆の間は狙いへ向き直り、跳びかかりなら空中の区間で着地点まで動く。
                // 判定そのものはCombatSystemがattackTimerを見て出す
                //-------------------------------------------------------------
                case BossActionPhase::Move: {
                    const BossMoveDef& move = set.moves[static_cast<size_t>(boss.currentMove)];

                    //-------------------------------------------------------------
                    // 旋回：プレイヤーを中心に、決めた距離を保ちながら横へ回る。時間が来たら隙を経て追跡へ戻る
                    //-------------------------------------------------------------
                    if(move.kind == BossMoveKind::Orbit) {
                        animSet.desiredState = EnemyAnimState::Walk;

                        hlslpp::float3 fromPlayer = transform.position - context.blackboard.playerPosition;
                        fromPlayer.y              = 0.0f;
                        const float length        = hlslpp::length(fromPlayer);
                        if(length > 1.0f) {
                            const hlslpp::float3 radial  = fromPlayer / length;
                            const hlslpp::float3 tangent = hlslpp::float3(-radial.z, 0.0f, radial.x) * boss.orbitSign;
                            // 半径からずれた分は内外へ寄せて戻す（ずれが大きいほど強く）
                            const float          orbitRadius = std::max(move.orbitRadius * boss.sizeScale, 1.0f);
                            const float          pull        = std::clamp((orbitRadius - length) / orbitRadius, -1.0f, 1.0f);
                            const hlslpp::float3 velocity    = tangent * move.orbitSpeed + radial * (pull * move.orbitSpeed);
                            transform.position               = transform.position + velocity * dt;
                            transform.dirty                  = true;
                            TurnToward(transform, transform.position + velocity, set.turnSpeed * dt);
                        }

                        if(boss.phaseTimer >= move.orbitSeconds) {
                            boss.phase      = BossActionPhase::Recovery;
                            boss.phaseTimer = 0.0f;
                            boss.cooldowns[static_cast<size_t>(boss.currentMove)] = move.cooldown * pace;
                        }
                        break;
                    }

                    animSet.desiredState    = EnemyAnimState::Attack;
                    animSet.attackTimer += dt;

                    // Attackへ切り替わった瞬間にattackTimerは0へ戻される。切り替わる前の1フレームは0として扱う
                    const float t = animSet.currentState == EnemyAnimState::Attack ? animSet.attackTimer : 0.0f;

                    const float windupEnd = move.kind == BossMoveKind::Charge ? move.chargeStart : move.hitbox.start;
                    if(t < windupEnd) {
                        const bool fixedTarget = move.area == BossAreaShape::DiscAtTarget;
                        TurnToward(transform, fixedTarget ? boss.moveTarget : context.blackboard.playerPosition, set.turnSpeed * dt);

                        // 弾の技は予兆の間に後ろへ下がって間合いを取る
                        if(move.kind == BossMoveKind::Projectile && move.retreatSpeed > 0.0f) {
                            const hlslpp::float3 forward = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), transform.rotation);
                            transform.position           = transform.position - forward * move.retreatSpeed * dt;
                            transform.dirty              = true;
                        }
                    }

                    //-------------------------------------------------------------
                    // 突進：駆け出す瞬間の向きで固定し、駆けている間は一直線に進む（途中で曲がらない＝見てから避けられる）
                    //-------------------------------------------------------------
                    if(move.kind == BossMoveKind::Charge && t >= move.chargeStart && t < move.chargeStart + move.chargeDuration) {
                        if(!boss.chargeLocked) {
                            boss.chargeLocked      = true;
                            boss.chargeDirection   = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), transform.rotation);
                            boss.chargeDirection.y = 0.0f;
                            const float length     = hlslpp::length(boss.chargeDirection);
                            boss.chargeDirection   = length > 0.001f ? boss.chargeDirection / length : hlslpp::float3(0.0f, 0.0f, 1.0f);
                        }
                        transform.position = transform.position + boss.chargeDirection * move.chargeSpeed * dt;
                        transform.dirty    = true;
                    }

                    //-------------------------------------------------------------
                    // 弾：判定の時刻にプレイヤーへ向けて扇状に撃つ。生成はツリーの外（BossDirectorSystem）
                    //-------------------------------------------------------------
                    if(move.kind == BossMoveKind::Projectile && !boss.summoned && t >= move.hitbox.start) {
                        boss.summoned = true;
                        if(auto* eventBus = context.registry.GetContext<Tsukino::ECS::EventBus*>()) {
                            hlslpp::float3 toPlayer = context.blackboard.playerPosition - transform.position;
                            toPlayer.y              = 0.0f;
                            const float baseYaw     = static_cast<float>(hlslpp::length(toPlayer)) > 0.001f
                                                          ? std::atan2(static_cast<float>(toPlayer.x), static_cast<float>(toPlayer.z))
                                                          : GetYaw(transform);
                            const int   count       = std::max(move.projectileCount, 1);
                            const float spreadRad   = move.projectileSpread * 0.0174533f;

                            // 口元あたりから撃つ（胴の高さ・前へ判定の半径ぶん）
                            const hlslpp::float3 forward = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), transform.rotation);
                            hlslpp::float3       origin  = transform.position + forward * (move.hitbox.radius * boss.sizeScale);
                            origin.y                     = transform.position.y + 40.0f * boss.sizeScale;

                            for(int i = 0; i < count; ++i) {
                                const float u   = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) - 0.5f : 0.0f;
                                const float yaw = baseYaw + u * spreadRad;

                                BossProjectileEvent shot;
                                shot.owner       = context.entity;
                                shot.origin      = origin;
                                shot.direction   = hlslpp::float3(std::sin(yaw), 0.0f, std::cos(yaw));
                                shot.speed       = move.projectileSpeed;
                                shot.damage      = move.projectileDamage * boss.damageScale;
                                shot.radius      = move.projectileRadius;
                                shot.range       = move.projectileRange;
                                shot.slowSeconds = move.slowSeconds;
                                shot.slowScale   = move.slowScale;
                                eventBus->Publish(shot);
                            }
                        }
                    }

                    if(move.leapStart >= 0.0f && move.leapEnd > move.leapStart && t >= move.leapStart) {
                        const float u      = std::clamp((t - move.leapStart) / (move.leapEnd - move.leapStart), 0.0f, 1.0f);
                        const float eased  = u * u * (3.0f - 2.0f * u);
                        hlslpp::float3 pos = boss.moveOrigin + (boss.moveTarget - boss.moveOrigin) * eased;
                        pos.y              = transform.position.y;
                        transform.position = pos;
                        transform.dirty    = true;
                    }

                    if(move.summonCount > 0 && !boss.summoned && t >= move.hitbox.start) {
                        boss.summoned = true;
                        if(auto* eventBus = context.registry.GetContext<Tsukino::ECS::EventBus*>())
                            eventBus->Publish(BossSummonEvent{transform.position, move.summonCount, move.summonRadius * boss.sizeScale, move.summonPrefab,
                                                              move.summonSizeScale, move.summonHealthScale});
                    }

                    const bool finished = t >= move.length || boss.phaseTimer >= move.length + kMoveTimeoutMargin;
                    if(finished) {
                        boss.phase      = BossActionPhase::Recovery;
                        boss.phaseTimer = 0.0f;
                        if(boss.repeatLeft <= 0)
                            boss.cooldowns[static_cast<size_t>(boss.currentMove)] = move.cooldown * pace;
                    }
                    break;
                }

                //-------------------------------------------------------------
                // 隙：その場で止まる。連続攻撃が残っていれば短い隙で同じ技をもう一度
                //-------------------------------------------------------------
                case BossActionPhase::Recovery: {
                    animSet.desiredState    = EnemyAnimState::Idle;
                    const BossMoveDef& move = set.moves[static_cast<size_t>(boss.currentMove)];
                    const float        wait = boss.repeatLeft > 0 ? kRepeatGap : move.recovery * pace;
                    if(boss.phaseTimer < wait)
                        break;

                    if(boss.repeatLeft > 0) {
                        --boss.repeatLeft;
                        StartMove(context, boss, set, boss.currentMove, false);
                    } else {
                        boss.phase       = BossActionPhase::Chase;
                        boss.phaseTimer  = 0.0f;
                        boss.currentMove = -1;
                    }
                    break;
                }

                //-------------------------------------------------------------
                // ダウン：その場で膝をついて動かない。受けるダメージはApplyBossHitModifiersが増やす。
                // 明けたらゲージを空にして追跡へ戻る（すぐに技を出せるよう、クールタイムはそのまま）
                //-------------------------------------------------------------
                case BossActionPhase::Broken: {
                    // StartBreakで待機を1フレーム挟んだので、ここからは攻撃ステートの仕組みでダウンのクリップを流す
                    animSet.desiredState = EnemyAnimState::Attack;
                    if(boss.phaseTimer < breakDef.downSeconds)
                        break;

                    boss.breakGauge      = 0.0f;
                    boss.breakIdleTimer  = 0.0f;
                    boss.phase           = BossActionPhase::Chase;
                    boss.phaseTimer      = 0.0f;
                    animSet.attackClipEnd = 0.0f;
                    animSet.desiredState  = EnemyAnimState::Idle;
                    break;
                }
            }

            return NodeStatus::Running;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief  ボス用のビヘイビアツリーを構築する
    //-------------------------------------------------------------
    std::shared_ptr<EnemyBehaviorNode> BuildBossTree() {
        auto root = std::make_shared<Selector<EnemyBlackboard>>();
        root->AddChild(BuildEnemyDeathSequence());
        root->AddChild(std::make_shared<ActionNode<EnemyBlackboard>>(&RunBoss));
        return root;
    }
}    // namespace CombatAndroid::ECS
