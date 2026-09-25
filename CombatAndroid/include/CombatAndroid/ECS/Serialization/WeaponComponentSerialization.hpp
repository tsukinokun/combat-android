//-------------------------------------------------------------
//! @file   WeaponComponentSerialization.hpp
//! @brief  武器まわりのComponent（Weapon・Pickup）のcerealシリアライズ定義
//! @note   Weaponは武器種ごとの調整値（持ち方・浮遊・判定・専用モーション・AoE・ノックバック・斬撃弾）だけを保存する。
//!         所有者・レベル・命中履歴・ばね/攻撃の進行などの実行時状態は保存しない。
//!         実ダメージ（damage）はWeaponTableからRecalculateWeaponStatsが導出するので書かない。
//!         C配列（attackStepStartTime等）は名前付きで1要素ずつ書く（AssetRefResolverArchiveがC配列を辿れないため）
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Utf8.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const WeaponComponent& weapon) {
        archive(cereal::make_nvp("weaponId", weapon.weaponId),
                cereal::make_nvp("handBoneName", weapon.handBoneName),
                cereal::make_nvp("localOffset", weapon.localOffset),
                cereal::make_nvp("gripRotationOffset", weapon.gripRotationOffset),
                cereal::make_nvp("handTrackingWeight", weapon.handTrackingWeight),
                cereal::make_nvp("attackHandTrackingWeight", weapon.attackHandTrackingWeight),
                cereal::make_nvp("attackLocalOffset", weapon.attackLocalOffset),
                cereal::make_nvp("attackGripRotationOffset", weapon.attackGripRotationOffset),
                cereal::make_nvp("gripPointLocal", weapon.gripPointLocal),
                cereal::make_nvp("floatBobAmplitude", weapon.floatBobAmplitude),
                cereal::make_nvp("floatBobSpeed", weapon.floatBobSpeed),
                cereal::make_nvp("floatDriftAmplitude", weapon.floatDriftAmplitude),
                cereal::make_nvp("floatDriftSpeed", weapon.floatDriftSpeed),
                cereal::make_nvp("floatSwayAngle", weapon.floatSwayAngle),
                cereal::make_nvp("floatSwaySpeed", weapon.floatSwaySpeed),
                cereal::make_nvp("floatSelectedHeightBoost", weapon.floatSelectedHeightBoost),
                cereal::make_nvp("damageMultiplier", weapon.damageMultiplier),
                cereal::make_nvp("range", weapon.range),
                cereal::make_nvp("hitCapsuleRadius", weapon.hitCapsuleRadius),
                cereal::make_nvp("activeDuration", weapon.activeDuration),
                cereal::make_nvp("cooldown", weapon.cooldown),
                cereal::make_nvp("attackBlendSpeed", weapon.attackBlendSpeed),
                cereal::make_nvp("attachPositionLerpSpeed", weapon.attachPositionLerpSpeed),
                cereal::make_nvp("attachRotationLerpSpeed", weapon.attachRotationLerpSpeed),
                cereal::make_nvp("followSpringFrequency", weapon.followSpringFrequency),
                cereal::make_nvp("followSpringDamping", weapon.followSpringDamping),
                cereal::make_nvp("attackSnapDistance", weapon.attackSnapDistance),
                cereal::make_nvp("attackSnapAngleDeg", weapon.attackSnapAngleDeg),
                cereal::make_nvp("attackApproachLerpSpeed", weapon.attackApproachLerpSpeed),
                cereal::make_nvp("areaAttackRadius", weapon.areaAttackRadius),
                cereal::make_nvp("areaAttackEffect", weapon.areaAttackEffectAsset),
                cereal::make_nvp("areaAttackEffectScale", weapon.areaAttackEffectScale),
                cereal::make_nvp("knockbackIgnoresThreshold", weapon.knockbackIgnoresThreshold),
                cereal::make_nvp("knockbackSpeed", weapon.knockbackSpeed),
                cereal::make_nvp("knockbackStun", weapon.knockbackStun),
                cereal::make_nvp("areaKnockbackSpeed", weapon.areaKnockbackSpeed),
                cereal::make_nvp("areaKnockbackStun", weapon.areaKnockbackStun),
                cereal::make_nvp("attackClip", weapon.attackClip),
                cereal::make_nvp("attackAnimationIndex", weapon.attackAnimationIndex),
                cereal::make_nvp("attackStepStartTime0", weapon.attackStepStartTime[0]),
                cereal::make_nvp("attackStepEndTime0", weapon.attackStepEndTime[0]),
                cereal::make_nvp("attackStepStartTime1", weapon.attackStepStartTime[1]),
                cereal::make_nvp("attackStepEndTime1", weapon.attackStepEndTime[1]),
                cereal::make_nvp("attackStepStartTime2", weapon.attackStepStartTime[2]),
                cereal::make_nvp("attackStepEndTime2", weapon.attackStepEndTime[2]),
                cereal::make_nvp("chargeAttackEnabled", weapon.chargeAttackEnabled),
                cereal::make_nvp("projectileEffect", weapon.projectileEffectAsset),
                cereal::make_nvp("projectileEffectScale", weapon.projectileEffectScale),
                cereal::make_nvp("projectileEffectPlaySpeed", weapon.projectileEffectPlaySpeed),
                cereal::make_nvp("projectileSpeed", weapon.projectileSpeed),
                cereal::make_nvp("projectileRadius", weapon.projectileRadius),
                cereal::make_nvp("projectileLifetime", weapon.projectileLifetime),
                cereal::make_nvp("projectileMaxDistance", weapon.projectileMaxDistance),
                cereal::make_nvp("projectileDamageMultiplier", weapon.projectileDamageMultiplier),
                cereal::make_nvp("projectileSpawnHeight", weapon.projectileSpawnHeight),
                cereal::make_nvp("projectileSpawnForward", weapon.projectileSpawnForward),
                cereal::make_nvp("projectilePierceMinChargeStage", weapon.projectilePierceMinChargeStage),
                cereal::make_nvp("projectileSpawnDelay", weapon.projectileSpawnDelay));
    }

    template <class Archive>
    void load(Archive& archive, WeaponComponent& weapon) {
        static_assert(PlayerAnimationSetComponent::kAttackComboCount == 3, "attackStep*0〜2の名前付き保存は3段を前提にしている");

        LoadField(archive, "weaponId", weapon.weaponId);
        LoadField(archive, "handBoneName", weapon.handBoneName);
        LoadField(archive, "localOffset", weapon.localOffset);
        LoadField(archive, "gripRotationOffset", weapon.gripRotationOffset);
        LoadField(archive, "handTrackingWeight", weapon.handTrackingWeight);
        LoadField(archive, "attackHandTrackingWeight", weapon.attackHandTrackingWeight);
        LoadField(archive, "attackLocalOffset", weapon.attackLocalOffset);
        LoadField(archive, "attackGripRotationOffset", weapon.attackGripRotationOffset);
        LoadField(archive, "gripPointLocal", weapon.gripPointLocal);
        LoadField(archive, "floatBobAmplitude", weapon.floatBobAmplitude);
        LoadField(archive, "floatBobSpeed", weapon.floatBobSpeed);
        LoadField(archive, "floatDriftAmplitude", weapon.floatDriftAmplitude);
        LoadField(archive, "floatDriftSpeed", weapon.floatDriftSpeed);
        LoadField(archive, "floatSwayAngle", weapon.floatSwayAngle);
        LoadField(archive, "floatSwaySpeed", weapon.floatSwaySpeed);
        LoadField(archive, "floatSelectedHeightBoost", weapon.floatSelectedHeightBoost);
        LoadField(archive, "damageMultiplier", weapon.damageMultiplier);
        LoadField(archive, "range", weapon.range);
        LoadField(archive, "hitCapsuleRadius", weapon.hitCapsuleRadius);
        LoadField(archive, "activeDuration", weapon.activeDuration);
        LoadField(archive, "cooldown", weapon.cooldown);
        LoadField(archive, "attackBlendSpeed", weapon.attackBlendSpeed);
        LoadField(archive, "attachPositionLerpSpeed", weapon.attachPositionLerpSpeed);
        LoadField(archive, "attachRotationLerpSpeed", weapon.attachRotationLerpSpeed);
        LoadField(archive, "followSpringFrequency", weapon.followSpringFrequency);
        LoadField(archive, "followSpringDamping", weapon.followSpringDamping);
        LoadField(archive, "attackSnapDistance", weapon.attackSnapDistance);
        LoadField(archive, "attackSnapAngleDeg", weapon.attackSnapAngleDeg);
        LoadField(archive, "attackApproachLerpSpeed", weapon.attackApproachLerpSpeed);
        LoadField(archive, "areaAttackRadius", weapon.areaAttackRadius);
        LoadField(archive, "areaAttackEffect", weapon.areaAttackEffectAsset);
        LoadField(archive, "areaAttackEffectScale", weapon.areaAttackEffectScale);
        LoadField(archive, "knockbackIgnoresThreshold", weapon.knockbackIgnoresThreshold);
        LoadField(archive, "knockbackSpeed", weapon.knockbackSpeed);
        LoadField(archive, "knockbackStun", weapon.knockbackStun);
        LoadField(archive, "areaKnockbackSpeed", weapon.areaKnockbackSpeed);
        LoadField(archive, "areaKnockbackStun", weapon.areaKnockbackStun);
        LoadField(archive, "attackClip", weapon.attackClip);
        LoadField(archive, "attackAnimationIndex", weapon.attackAnimationIndex);
        LoadField(archive, "attackStepStartTime0", weapon.attackStepStartTime[0]);
        LoadField(archive, "attackStepEndTime0", weapon.attackStepEndTime[0]);
        LoadField(archive, "attackStepStartTime1", weapon.attackStepStartTime[1]);
        LoadField(archive, "attackStepEndTime1", weapon.attackStepEndTime[1]);
        LoadField(archive, "attackStepStartTime2", weapon.attackStepStartTime[2]);
        LoadField(archive, "attackStepEndTime2", weapon.attackStepEndTime[2]);
        LoadField(archive, "chargeAttackEnabled", weapon.chargeAttackEnabled);
        LoadField(archive, "projectileEffect", weapon.projectileEffectAsset);
        LoadField(archive, "projectileEffectScale", weapon.projectileEffectScale);
        LoadField(archive, "projectileEffectPlaySpeed", weapon.projectileEffectPlaySpeed);
        LoadField(archive, "projectileSpeed", weapon.projectileSpeed);
        LoadField(archive, "projectileRadius", weapon.projectileRadius);
        LoadField(archive, "projectileLifetime", weapon.projectileLifetime);
        LoadField(archive, "projectileMaxDistance", weapon.projectileMaxDistance);
        LoadField(archive, "projectileDamageMultiplier", weapon.projectileDamageMultiplier);
        LoadField(archive, "projectileSpawnHeight", weapon.projectileSpawnHeight);
        LoadField(archive, "projectileSpawnForward", weapon.projectileSpawnForward);
        LoadField(archive, "projectilePierceMinChargeStage", weapon.projectilePierceMinChargeStage);
        LoadField(archive, "projectileSpawnDelay", weapon.projectileSpawnDelay);

        // EffectSystem::PlayEffect（テクスチャの解決）が使うパスは、AssetRefのパスから作る
        if constexpr(kIsRealInputArchive<Archive>) {
            weapon.areaAttackEffectPath = Tsukino::Core::Path(weapon.areaAttackEffectAsset.path);
            weapon.projectileEffectPath = Tsukino::Core::Path(weapon.projectileEffectAsset.path);
        }
    }

    template <class Archive>
    void save(Archive& archive, const PickupComponent& pickup) {
        archive(cereal::make_nvp("radius", pickup.radius),
                cereal::make_nvp("displayName", WideToUtf8(pickup.displayName)),
                cereal::make_nvp("labelHeight", pickup.labelHeight));
    }

    template <class Archive>
    void load(Archive& archive, PickupComponent& pickup) {
        // 表示名はJSON上ではUTF-8。解決用の擬似アーカイブでの再訪問では読み直さない
        if constexpr(kIsRealInputArchive<Archive>) {
            std::string displayNameUtf8 = WideToUtf8(pickup.displayName);

            LoadField(archive, "radius", pickup.radius);
            LoadField(archive, "displayName", displayNameUtf8);
            LoadField(archive, "labelHeight", pickup.labelHeight);

            pickup.displayName = Utf8ToWide(displayNameUtf8);
        }
    }
}    // namespace CombatAndroid::ECS
