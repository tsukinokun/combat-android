//-------------------------------------------------------------
//! @file    CombatHit.cpp
//! @brief   敵1体へのヒットを確定させる共通処理の実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/CombatHit.hpp>

#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/HitStopComponent.hpp>
#include <CombatAndroid/ECS/Event/WeaponHitEvent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief 1体のエンティティへヒットストップを要求/更新する
    //-------------------------------------------------------------
    void ApplyHitStop(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float duration, float scale) {
        if(entity == entt::null)
            return;

        bool  alreadyActive = registry.HasComponent<HitStopComponent>(entity);
        auto& hitStop       = alreadyActive ? registry.GetComponent<HitStopComponent>(entity) : registry.AddComponent<HitStopComponent>(entity);

        // 新規発動時のみ、その時点のplayback_speedを基準値として保存する。
        // 既に発動中（同一攻撃の多段ヒット等）なら、HitStopSystemが既に減速させた後の
        // 値を基準に取り直してしまわないよう、最初に保存した基準値を保ち続ける
        if(!alreadyActive) {
            if(auto* animPlayer = registry.try_get<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity))
                hitStop.baseAnimSpeed = animPlayer->playback_speed;
            else
                hitStop.baseAnimSpeed = 1.0f;
        }

        // 同一フレームで複数回要求されても同じ値で上書きされるだけで問題ない
        hitStop.remainingTime = duration;
        hitStop.scale         = scale;
    }

    //-------------------------------------------------------------
    //! @brief 1体の敵へのヒットを確定させる
    //-------------------------------------------------------------
    bool ApplyCombatHit(Tsukino::ECS::Registry& registry,
                        Tsukino::ECS::EventBus* eventBus,
                        Tsukino::ECS::Entity attacker,
                        Tsukino::ECS::Entity sourceEntity,
                        Tsukino::ECS::Entity hitEntity,
                        float dealtDamage,
                        const hlslpp::float3& hitPositionFallback,
                        std::vector<Tsukino::ECS::Entity>& hitRecord,
                        float lifeStealRatio,
                        float& lifeStealHealed) {
        if(!registry.HasComponent<EnemyComponent>(hitEntity) || !registry.HasComponent<HealthComponent>(hitEntity))
            return false;

        auto& enemy       = registry.GetComponent<EnemyComponent>(hitEntity);
        auto& enemyHealth = registry.GetComponent<HealthComponent>(hitEntity);
        if(enemyHealth.isDead)
            return false;

        if(std::find(hitRecord.begin(), hitRecord.end(), hitEntity) != hitRecord.end())
            return false;

        enemyHealth.currentHealth -= dealtDamage;
        if(enemyHealth.currentHealth <= 0.0f) {
            enemyHealth.currentHealth = 0.0f;
            enemyHealth.isDead        = true;
        }
        enemyHealth.hpBarVisibleTimer = kHpBarVisibleDuration;    // 被弾した瞬間だけ頭上HPバーを表示する
        hitRecord.push_back(hitEntity);

        // 一定以上の単発ダメージでノックバックを要求する（BTのPlayKnockbackが消費する）。
        // 既に硬直中なら再要求しない＝連撃で仰け反り続けるハメを防ぐ
        if(!enemy.isKnockedBack && dealtDamage >= enemy.knockbackDamageThreshold)
            enemy.pendingKnockback = true;

        //---------------------------------------------------------
        // 嫉妬（スキル）：与えたダメージの一部を攻撃者のHPへ変換する。
        // AoEや貫通する斬撃弾が群れを巻き込んだ一撃で全快を何周もしないよう、
        // アタック単位で総量を頭打ちにする
        //---------------------------------------------------------
        if(lifeStealRatio > 0.0f && attacker != entt::null) {
            if(auto* attackerHealth = registry.try_get<HealthComponent>(attacker)) {
                if(!attackerHealth->isDead) {
                    const float capThisAttack = attackerHealth->maxHealth * kLifeStealCapRatioPerAttack;
                    const float allowance     = std::max(capThisAttack - lifeStealHealed, 0.0f);
                    const float healAmount    = std::min(dealtDamage * lifeStealRatio, allowance);

                    // 実際に増えた分だけを上限の消費として計上する
                    // （HPが満タンで回復しきれなかった分まで消費すると、続くヒットが不当に吸収できなくなる）
                    const float healthBefore      = attackerHealth->currentHealth;
                    attackerHealth->currentHealth = std::min(attackerHealth->currentHealth + healAmount, attackerHealth->maxHealth);
                    lifeStealHealed += attackerHealth->currentHealth - healthBefore;
                }
            }
        }

        hlslpp::float3 hitPosition = hitPositionFallback;
        if(registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(hitEntity))
            hitPosition = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(hitEntity).position;

        // ヒット通知（エフェクト・SE等の副作用処理用）を発火する
        if(eventBus) {
            eventBus->Publish(WeaponHitEvent{attacker, sourceEntity, hitEntity, hitPosition, dealtDamage});
        }

        // ヒットストップを要求する。画面全体ではなく、被弾した敵と攻撃者だけを止める
        ApplyHitStop(registry, hitEntity, kHitStopDuration, kHitStopScale);
        ApplyHitStop(registry, attacker, kHitStopDuration, kHitStopScale);

        return true;
    }
}    // namespace CombatAndroid::ECS
