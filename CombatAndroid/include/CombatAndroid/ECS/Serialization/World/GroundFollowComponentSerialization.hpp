//-------------------------------------------------------------
//! @file   GroundFollowComponentSerialization.hpp
//! @brief  GroundFollowComponentのcerealシリアライズ定義
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/World/GroundFollowComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const GroundFollowComponent& follow) {
        archive(cereal::make_nvp("groundHeight", follow.groundHeight));
    }

    template <class Archive>
    void load(Archive& archive, GroundFollowComponent& follow) {
        LoadField(archive, "groundHeight", follow.groundHeight);
    }
}    // namespace CombatAndroid::ECS
