//-------------------------------------------------------------
//! @file   TreeFieldComponentSerialization.hpp
//! @brief  TreeFieldComponentのcerealシリアライズ定義
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/World/TreeFieldComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const TreeFieldComponent& trees) {
        archive(cereal::make_nvp("enabled", trees.enabled),
                cereal::make_nvp("seed", trees.seed),
                cereal::make_nvp("variantCount", trees.variantCount),
                cereal::make_nvp("cellSize", trees.cellSize),
                cereal::make_nvp("spawnChance", trees.spawnChance),
                cereal::make_nvp("cellJitter", trees.cellJitter),
                cereal::make_nvp("drawRadius", trees.drawRadius),
                cereal::make_nvp("fadeBand", trees.fadeBand),
                cereal::make_nvp("startClearRadius", trees.startClearRadius),
                cereal::make_nvp("scaleMin", trees.scaleMin),
                cereal::make_nvp("scaleMax", trees.scaleMax),
                cereal::make_nvp("colliderRadius", trees.colliderRadius),
                cereal::make_nvp("colliderRadiusScale", trees.colliderRadiusScale),
                cereal::make_nvp("colliderHeight", trees.colliderHeight),
                cereal::make_nvp("trunkHeightMin", trees.trunkHeightMin),
                cereal::make_nvp("trunkHeightMax", trees.trunkHeightMax),
                cereal::make_nvp("trunkRadiusMin", trees.trunkRadiusMin),
                cereal::make_nvp("trunkRadiusMax", trees.trunkRadiusMax),
                cereal::make_nvp("branchDepth", trees.branchDepth),
                cereal::make_nvp("childCountMin", trees.childCountMin),
                cereal::make_nvp("childCountMax", trees.childCountMax),
                cereal::make_nvp("spreadAngleMin", trees.spreadAngleMin),
                cereal::make_nvp("spreadAngleMax", trees.spreadAngleMax),
                cereal::make_nvp("canopyRadiusMin", trees.canopyRadiusMin),
                cereal::make_nvp("canopyRadiusMax", trees.canopyRadiusMax),
                cereal::make_nvp("leafCardsPerTip", trees.leafCardsPerTip),
                cereal::make_nvp("leafCardSize", trees.leafCardSize),
                cereal::make_nvp("barkBottomColor", trees.barkBottomColor),
                cereal::make_nvp("barkTopColor", trees.barkTopColor),
                cereal::make_nvp("leafBottomColor", trees.leafBottomColor),
                cereal::make_nvp("leafTopColor", trees.leafTopColor));
    }

    template <class Archive>
    void load(Archive& archive, TreeFieldComponent& trees) {
        LoadField(archive, "enabled", trees.enabled);
        LoadField(archive, "seed", trees.seed);
        LoadField(archive, "variantCount", trees.variantCount);
        LoadField(archive, "cellSize", trees.cellSize);
        LoadField(archive, "spawnChance", trees.spawnChance);
        LoadField(archive, "cellJitter", trees.cellJitter);
        LoadField(archive, "drawRadius", trees.drawRadius);
        LoadField(archive, "fadeBand", trees.fadeBand);
        LoadField(archive, "startClearRadius", trees.startClearRadius);
        LoadField(archive, "scaleMin", trees.scaleMin);
        LoadField(archive, "scaleMax", trees.scaleMax);
        LoadField(archive, "colliderRadius", trees.colliderRadius);
        LoadField(archive, "colliderRadiusScale", trees.colliderRadiusScale);
        LoadField(archive, "colliderHeight", trees.colliderHeight);
        LoadField(archive, "trunkHeightMin", trees.trunkHeightMin);
        LoadField(archive, "trunkHeightMax", trees.trunkHeightMax);
        LoadField(archive, "trunkRadiusMin", trees.trunkRadiusMin);
        LoadField(archive, "trunkRadiusMax", trees.trunkRadiusMax);
        LoadField(archive, "branchDepth", trees.branchDepth);
        LoadField(archive, "childCountMin", trees.childCountMin);
        LoadField(archive, "childCountMax", trees.childCountMax);
        LoadField(archive, "spreadAngleMin", trees.spreadAngleMin);
        LoadField(archive, "spreadAngleMax", trees.spreadAngleMax);
        LoadField(archive, "canopyRadiusMin", trees.canopyRadiusMin);
        LoadField(archive, "canopyRadiusMax", trees.canopyRadiusMax);
        LoadField(archive, "leafCardsPerTip", trees.leafCardsPerTip);
        LoadField(archive, "leafCardSize", trees.leafCardSize);
        LoadField(archive, "barkBottomColor", trees.barkBottomColor);
        LoadField(archive, "barkTopColor", trees.barkTopColor);
        LoadField(archive, "leafBottomColor", trees.leafBottomColor);
        LoadField(archive, "leafTopColor", trees.leafTopColor);
    }
}    // namespace CombatAndroid::ECS
