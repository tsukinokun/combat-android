//-------------------------------------------------------------
//! @file   WorldAnchorComponentSerialization.hpp
//! @brief  WorldAnchorComponentのcerealシリアライズ定義（エンジン未対応のためゲーム側で持つ）
//! @note   貼り付け方（オフセット・固定位置）だけを保存する。target（Entity）と visible は実行時状態
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : Tsukino::BuiltIn::ECS
namespace Tsukino::BuiltIn::ECS {
    template <class Archive>
    void save(Archive& archive, const WorldAnchorComponent& anchor) {
        archive(cereal::make_nvp("useFixedWorldPosition", anchor.useFixedWorldPosition),
                cereal::make_nvp("fixedWorldPosition", anchor.fixedWorldPosition),
                cereal::make_nvp("worldOffset", anchor.worldOffset),
                cereal::make_nvp("screenOffset", anchor.screenOffset));
    }

    template <class Archive>
    void load(Archive& archive, WorldAnchorComponent& anchor) {
        CombatAndroid::ECS::LoadField(archive, "useFixedWorldPosition", anchor.useFixedWorldPosition);
        CombatAndroid::ECS::LoadField(archive, "fixedWorldPosition", anchor.fixedWorldPosition);
        CombatAndroid::ECS::LoadField(archive, "worldOffset", anchor.worldOffset);
        CombatAndroid::ECS::LoadField(archive, "screenOffset", anchor.screenOffset);
    }
}    // namespace Tsukino::BuiltIn::ECS
