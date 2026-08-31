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
#include <cmath>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief 敵1体へノックバックを要求する
    //-------------------------------------------------------------
    void RequestKnockback(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity enemyEntity, const KnockbackParams& params) {
        auto* enemy = registry.try_get<EnemyComponent>(enemyEntity);
        if(!enemy)
            return;

        // 硬直中は「今より強い要求」だけを通す。弱い要求まで通すと、連撃のたびに
        // のけぞりが頭から再生され直して永久に硬直するハメになる
        if(enemy->isKnockedBack && params.speed <= enemy->knockbackStrength)
            return;

        //---------------------------------------------------------
        // 押し出す向き：起点（プレイヤー）から対象へ向かう水平方向。
        // 起点と対象がほぼ重なっている場合は向きが定まらないので、
        // 対象の背面方向（＝自分から見た後ろ）へ逃がす
        //---------------------------------------------------------
        hlslpp::float3 direction(0.0f, 0.0f, 0.0f);
        if(params.speed > 0.0f) {
            if(const auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(enemyEntity)) {
                hlslpp::float3 away = transform->position - params.sourcePosition;
                away.y              = 0.0f;    // 水平のみ。浮かせると接地高さの管理が要るため今は上へは飛ばさない

                float awayLength = hlslpp::length(away);
                if(awayLength > 1e-4f)
                    direction = away / awayLength;
                else
                    direction = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, -1.0f), transform->rotation);
            }
        }

        enemy->pendingKnockback         = true;
        enemy->pendingKnockbackVelocity = direction * params.speed;
        enemy->pendingKnockbackStun     = params.stunDuration;
        enemy->knockbackStrength        = params.speed;
    }

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
                        float& lifeStealHealed,
                        const KnockbackParams& knockback) {
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
        // 重い武器（KnockbackParams::ignoreDamageThreshold＝greatsword等）はダメージ量に関わらず
        // 必ず怯ませる。これはEnemyDifficultyTable::knockbackThresholdScaleによる難易度スケールも
        // 一緒にバイパスするが、「重い武器は難易度に関わらず怯ませる」ことを狙った意図的な挙動。
        // 硬直中の再要求を弾くハメ防止はRequestKnockback側（強い要求だけが勝つ）が持つ
        if(knockback.ignoreDamageThreshold || dealtDamage >= enemy.knockbackDamageThreshold)
            RequestKnockback(registry, hitEntity, knockback);

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
