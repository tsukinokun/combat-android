//-------------------------------------------------------------
//! @file   CharacterControllerComponentSerialization.hpp
//! @brief  CharacterControllerComponentのcerealシリアライズ定義（エンジン未対応のためゲーム側で持つ）
//! @note   調整値（形状・重力など）だけを持つ。moveInput・verticalVelocity・isInitializedは実行時状態なので保存しない
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : Tsukino::BuiltIn::ECS
namespace Tsukino::BuiltIn::ECS {
    template <class Archive>
    void save(Archive& archive, const CharacterControllerComponent& controller) {
        archive(cereal::make_nvp("radius", controller.radius),
                cereal::make_nvp("halfHeight", controller.halfHeight),
                cereal::make_nvp("maxSlopeDeg", controller.maxSlopeDeg),
                cereal::make_nvp("mass", controller.mass),
                cereal::make_nvp("gravityFactor", controller.gravityFactor),
                cereal::make_nvp("centerOffset", controller.centerOffset));
    }

    template <class Archive>
    void load(Archive& archive, CharacterControllerComponent& controller) {
        CombatAndroid::ECS::LoadField(archive, "radius", controller.radius);
        CombatAndroid::ECS::LoadField(archive, "halfHeight", controller.halfHeight);
        CombatAndroid::ECS::LoadField(archive, "maxSlopeDeg", controller.maxSlopeDeg);
        CombatAndroid::ECS::LoadField(archive, "mass", controller.mass);
        CombatAndroid::ECS::LoadField(archive, "gravityFactor", controller.gravityFactor);
        CombatAndroid::ECS::LoadField(archive, "centerOffset", controller.centerOffset);
    }
}    // namespace Tsukino::BuiltIn::ECS
