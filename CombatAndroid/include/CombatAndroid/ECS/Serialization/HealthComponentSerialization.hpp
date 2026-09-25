//-------------------------------------------------------------
//! @file   HealthComponentSerialization.hpp
//! @brief  HealthComponentのcerealシリアライズ定義
//! @note   HPだけを保存する。HPバーのEntityハンドルとタイマーは実行時状態（スポーンした側が繋ぐ）
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const HealthComponent& health) {
        archive(cereal::make_nvp("maxHealth", health.maxHealth), cereal::make_nvp("currentHealth", health.currentHealth));
    }

    template <class Archive>
    void load(Archive& archive, HealthComponent& health) {
        LoadField(archive, "maxHealth", health.maxHealth);
        LoadField(archive, "currentHealth", health.currentHealth);
    }
}    // namespace CombatAndroid::ECS
