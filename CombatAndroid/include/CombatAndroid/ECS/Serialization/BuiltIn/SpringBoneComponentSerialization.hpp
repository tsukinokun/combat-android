//-------------------------------------------------------------
//! @file   SpringBoneComponentSerialization.hpp
//! @brief  SpringBoneComponent（の定義部）のcerealシリアライズ定義（エンジン未対応のためゲーム側で持つ）
//! @note   人間が設定する chainDefs と enabled だけを保存する。chains / resolved は実行時状態
//!         （AnimationSystemがchainDefsから自動生成する）
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/BuiltIn/ECS/Component/SpringBoneComponent.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

// 名前空間 : Tsukino::BuiltIn::ECS
namespace Tsukino::BuiltIn::ECS {
    template <class Archive>
    void save(Archive& archive, const SpringBoneComponent::ColliderDef& collider) {
        archive(cereal::make_nvp("attachNodeName", collider.attachNodeName),
                cereal::make_nvp("localOffset", collider.localOffset),
                cereal::make_nvp("radius", collider.radius));
    }

    template <class Archive>
    void load(Archive& archive, SpringBoneComponent::ColliderDef& collider) {
        CombatAndroid::ECS::LoadField(archive, "attachNodeName", collider.attachNodeName);
        CombatAndroid::ECS::LoadField(archive, "localOffset", collider.localOffset);
        CombatAndroid::ECS::LoadField(archive, "radius", collider.radius);
    }

    //-------------------------------------------------------------
    // ChainDef。settings（SpringBoneSettings）は位置指定のserializeしか持たず、JSONが読みにくいため
    // ここで項目名を付けて平らに書く
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const SpringBoneComponent::ChainDef& chain) {
        archive(cereal::make_nvp("name", chain.name),
                cereal::make_nvp("anchorNodeName", chain.anchorNodeName),
                cereal::make_nvp("rootNodeName", chain.rootNodeName),
                cereal::make_nvp("excludeNodeNames", chain.excludeNodeNames),
                cereal::make_nvp("maxDepth", chain.maxDepth),
                cereal::make_nvp("stiffness", chain.settings.stiffness),
                cereal::make_nvp("drag", chain.settings.drag),
                cereal::make_nvp("inertia", chain.settings.inertia),
                cereal::make_nvp("gravityScale", chain.settings.gravityScale),
                cereal::make_nvp("gravityDir", chain.settings.gravityDir),
                cereal::make_nvp("boneRadius", chain.settings.boneRadius),
                cereal::make_nvp("angleLimitDeg", chain.settings.angleLimitDeg),
                cereal::make_nvp("collisionIterations", chain.settings.collisionIterations),
                cereal::make_nvp("colliders", chain.colliders));
    }

    template <class Archive>
    void load(Archive& archive, SpringBoneComponent::ChainDef& chain) {
        CombatAndroid::ECS::LoadField(archive, "name", chain.name);
        CombatAndroid::ECS::LoadField(archive, "anchorNodeName", chain.anchorNodeName);
        CombatAndroid::ECS::LoadField(archive, "rootNodeName", chain.rootNodeName);
        CombatAndroid::ECS::LoadField(archive, "excludeNodeNames", chain.excludeNodeNames);
        CombatAndroid::ECS::LoadField(archive, "maxDepth", chain.maxDepth);
        CombatAndroid::ECS::LoadField(archive, "stiffness", chain.settings.stiffness);
        CombatAndroid::ECS::LoadField(archive, "drag", chain.settings.drag);
        CombatAndroid::ECS::LoadField(archive, "inertia", chain.settings.inertia);
        CombatAndroid::ECS::LoadField(archive, "gravityScale", chain.settings.gravityScale);
        CombatAndroid::ECS::LoadField(archive, "gravityDir", chain.settings.gravityDir);
        CombatAndroid::ECS::LoadField(archive, "boneRadius", chain.settings.boneRadius);
        CombatAndroid::ECS::LoadField(archive, "angleLimitDeg", chain.settings.angleLimitDeg);
        CombatAndroid::ECS::LoadField(archive, "collisionIterations", chain.settings.collisionIterations);
        CombatAndroid::ECS::LoadField(archive, "colliders", chain.colliders);
    }

    template <class Archive>
    void save(Archive& archive, const SpringBoneComponent& spring) {
        archive(cereal::make_nvp("enabled", spring.enabled), cereal::make_nvp("chainDefs", spring.chainDefs));
    }

    template <class Archive>
    void load(Archive& archive, SpringBoneComponent& spring) {
        // 参照（AssetRef/EntityRef）を含まないので、解決用の擬似アーカイブでの再訪問では何もしない
        if constexpr(CombatAndroid::ECS::kIsRealInputArchive<Archive>) {
            CombatAndroid::ECS::LoadField(archive, "enabled", spring.enabled);
            CombatAndroid::ECS::LoadField(archive, "chainDefs", spring.chainDefs);
        }
    }
}    // namespace Tsukino::BuiltIn::ECS
