//-------------------------------------------------------------
//! @file   EnemyComponentSerialization.hpp
//! @brief  敵まわりのComponent（Enemy・攻撃判定・アニメーション一式）のcerealシリアライズ定義
//! @note   調整値だけを保存する。ノックバック・死亡フェード・判定の解決キャッシュ・ステートなどの
//!         実行時状態は保存しない
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const EnemyComponent& enemy) {
        archive(cereal::make_nvp("moveSpeed", enemy.moveSpeed),
                cereal::make_nvp("detectRange", enemy.detectRange),
                cereal::make_nvp("bodyRadius", enemy.bodyRadius),
                cereal::make_nvp("attackRange", enemy.attackRange),
                cereal::make_nvp("attackCooldown", enemy.attackCooldown),
                cereal::make_nvp("expReward", enemy.expReward),
                cereal::make_nvp("knockbackDamageThreshold", enemy.knockbackDamageThreshold),
                cereal::make_nvp("knockbackDecayRate", enemy.knockbackDecayRate),
                cereal::make_nvp("deathFadeDuration", enemy.deathFadeDuration));
    }

    template <class Archive>
    void load(Archive& archive, EnemyComponent& enemy) {
        LoadField(archive, "moveSpeed", enemy.moveSpeed);
        LoadField(archive, "detectRange", enemy.detectRange);
        LoadField(archive, "bodyRadius", enemy.bodyRadius);
        LoadField(archive, "attackRange", enemy.attackRange);
        LoadField(archive, "attackCooldown", enemy.attackCooldown);
        LoadField(archive, "expReward", enemy.expReward);
        LoadField(archive, "knockbackDamageThreshold", enemy.knockbackDamageThreshold);
        LoadField(archive, "knockbackDecayRate", enemy.knockbackDecayRate);
        LoadField(archive, "deathFadeDuration", enemy.deathFadeDuration);
    }

    template <class Archive>
    void save(Archive& archive, const EnemyAttackHitboxComponent& hitbox) {
        archive(cereal::make_nvp("boneName", hitbox.boneName),
                cereal::make_nvp("boneLocalOffset", hitbox.boneLocalOffset),
                cereal::make_nvp("endBoneName", hitbox.endBoneName),
                cereal::make_nvp("endBoneLocalOffset", hitbox.endBoneLocalOffset),
                cereal::make_nvp("radius", hitbox.radius),
                cereal::make_nvp("damage", hitbox.damage),
                cereal::make_nvp("hitStartTime", hitbox.hitStartTime),
                cereal::make_nvp("hitDuration", hitbox.hitDuration));
    }

    template <class Archive>
    void load(Archive& archive, EnemyAttackHitboxComponent& hitbox) {
        LoadField(archive, "boneName", hitbox.boneName);
        LoadField(archive, "boneLocalOffset", hitbox.boneLocalOffset);
        LoadField(archive, "endBoneName", hitbox.endBoneName);
        LoadField(archive, "endBoneLocalOffset", hitbox.endBoneLocalOffset);
        LoadField(archive, "radius", hitbox.radius);
        LoadField(archive, "damage", hitbox.damage);
        LoadField(archive, "hitStartTime", hitbox.hitStartTime);
        LoadField(archive, "hitDuration", hitbox.hitDuration);
    }

    template <class Archive>
    void save(Archive& archive, const EnemyAnimationSetComponent& set) {
        archive(cereal::make_nvp("walkClip", set.walkClip),
                cereal::make_nvp("attackClip", set.attackClip),
                cereal::make_nvp("knockbackClip", set.knockbackClip),
                cereal::make_nvp("deathClip", set.deathClip),
                cereal::make_nvp("animationIndex", set.animationIndex),
                cereal::make_nvp("attackTimeoutSafety", set.attackTimeoutSafety),
                cereal::make_nvp("knockbackTimeoutSafety", set.knockbackTimeoutSafety),
                cereal::make_nvp("deathTimeoutSafety", set.deathTimeoutSafety));
    }

    template <class Archive>
    void load(Archive& archive, EnemyAnimationSetComponent& set) {
        LoadField(archive, "walkClip", set.walkClip);
        LoadField(archive, "attackClip", set.attackClip);
        LoadField(archive, "knockbackClip", set.knockbackClip);
        LoadField(archive, "deathClip", set.deathClip);
        LoadField(archive, "animationIndex", set.animationIndex);
        LoadField(archive, "attackTimeoutSafety", set.attackTimeoutSafety);
        LoadField(archive, "knockbackTimeoutSafety", set.knockbackTimeoutSafety);
        LoadField(archive, "deathTimeoutSafety", set.deathTimeoutSafety);
    }
}    // namespace CombatAndroid::ECS
