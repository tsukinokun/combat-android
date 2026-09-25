//-------------------------------------------------------------
//! @file   RimGlowComponentSerialization.hpp
//! @brief  RimGlowComponentのcerealシリアライズ定義（エンジン未対応のためゲーム側で持つ）
//! @note   save/loadはADLで見つけるためComponentと同じ名前空間に置く。エンジン側に同等の定義が入ったら削除する
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : Tsukino::BuiltIn::ECS
namespace Tsukino::BuiltIn::ECS {
    template <class Archive>
    void save(Archive& archive, const RimGlowComponent& rim) {
        archive(cereal::make_nvp("active", rim.active),
                cereal::make_nvp("rimColor", rim.rimColor),
                cereal::make_nvp("rimIntensity", rim.rimIntensity),
                cereal::make_nvp("rimPower", rim.rimPower),
                cereal::make_nvp("glow", rim.glow));
    }

    template <class Archive>
    void load(Archive& archive, RimGlowComponent& rim) {
        CombatAndroid::ECS::LoadField(archive, "active", rim.active);
        CombatAndroid::ECS::LoadField(archive, "rimColor", rim.rimColor);
        CombatAndroid::ECS::LoadField(archive, "rimIntensity", rim.rimIntensity);
        CombatAndroid::ECS::LoadField(archive, "rimPower", rim.rimPower);
        CombatAndroid::ECS::LoadField(archive, "glow", rim.glow);
    }
}    // namespace Tsukino::BuiltIn::ECS
