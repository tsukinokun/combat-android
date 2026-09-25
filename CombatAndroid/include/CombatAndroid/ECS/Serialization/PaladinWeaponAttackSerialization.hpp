//-------------------------------------------------------------
//! @file   PaladinWeaponAttackSerialization.hpp
//! @brief  PaladinWeaponAttack（Paladinが持つ武器ごとの攻撃パラメータ）のcerealシリアライズ定義
//! @note   Assets/Prefabs/Enemy/PaladinWeaponAttacks.json（武器名をキーにした入れ子）が持つ。
//!         weaponIdは書かず、読む側（どのキーか）が決める
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const PaladinWeaponAttack& attack) {
        archive(cereal::make_nvp("attackClipPath", attack.attackClipPath),
                cereal::make_nvp("attackRange", attack.attackRange),
                cereal::make_nvp("hitboxReach", attack.hitboxReach),
                cereal::make_nvp("hitboxRadius", attack.hitboxRadius),
                cereal::make_nvp("hitboxDamage", attack.hitboxDamage),
                cereal::make_nvp("hitStartTime", attack.hitStartTime),
                cereal::make_nvp("hitDuration", attack.hitDuration));
    }

    template <class Archive>
    void load(Archive& archive, PaladinWeaponAttack& attack) {
        LoadField(archive, "attackClipPath", attack.attackClipPath);
        LoadField(archive, "attackRange", attack.attackRange);
        LoadField(archive, "hitboxReach", attack.hitboxReach);
        LoadField(archive, "hitboxRadius", attack.hitboxRadius);
        LoadField(archive, "hitboxDamage", attack.hitboxDamage);
        LoadField(archive, "hitStartTime", attack.hitStartTime);
        LoadField(archive, "hitDuration", attack.hitDuration);
    }
}    // namespace CombatAndroid::ECS
