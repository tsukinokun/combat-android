//-------------------------------------------------------------
//! @file   GrassFieldComponentSerialization.hpp
//! @brief  GrassFieldComponent（と草の種類）のcerealシリアライズ定義
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/GrassFieldComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const GrassSpecies& species) {
        archive(cereal::make_nvp("height", species.height),
                cereal::make_nvp("widthScale", species.widthScale),
                cereal::make_nvp("rootColor", species.rootColor),
                cereal::make_nvp("tipColor", species.tipColor));
    }

    template <class Archive>
    void load(Archive& archive, GrassSpecies& species) {
        LoadField(archive, "height", species.height);
        LoadField(archive, "widthScale", species.widthScale);
        LoadField(archive, "rootColor", species.rootColor);
        LoadField(archive, "tipColor", species.tipColor);
    }

    template <class Archive>
    void save(Archive& archive, const GrassFieldComponent& grass) {
        archive(cereal::make_nvp("enabled", grass.enabled),
                cereal::make_nvp("bladeCount", grass.bladeCount),
                cereal::make_nvp("fieldSize", grass.fieldSize),
                cereal::make_nvp("seed", grass.seed),
                cereal::make_nvp("farFieldSize", grass.farFieldSize),
                cereal::make_nvp("farBladeCount", grass.farBladeCount),
                cereal::make_nvp("lodBlendStart", grass.lodBlendStart),
                cereal::make_nvp("lodBlendEnd", grass.lodBlendEnd),
                cereal::make_nvp("horizonFillStart", grass.horizonFillStart),
                cereal::make_nvp("horizonFillEnd", grass.horizonFillEnd),
                cereal::make_nvp("species", grass.species),
                cereal::make_nvp("bladeWidth", grass.bladeWidth),
                cereal::make_nvp("heightVariance", grass.heightVariance),
                cereal::make_nvp("groundHeight", grass.groundHeight),
                cereal::make_nvp("distantWidthBoost", grass.distantWidthBoost),
                cereal::make_nvp("clumpRadiusMin", grass.clumpRadiusMin),
                cereal::make_nvp("clumpRadiusMax", grass.clumpRadiusMax),
                cereal::make_nvp("clumpSpawnChance", grass.clumpSpawnChance),
                cereal::make_nvp("clumpShapeNoise", grass.clumpShapeNoise),
                cereal::make_nvp("clumpEdgeSoftness", grass.clumpEdgeSoftness),
                cereal::make_nvp("fillerDensity", grass.fillerDensity),
                cereal::make_nvp("fillerHeightScale", grass.fillerHeightScale),
                cereal::make_nvp("windDirection", grass.windDirection),
                cereal::make_nvp("windStrength", grass.windStrength),
                cereal::make_nvp("gustWavelength", grass.gustWavelength),
                cereal::make_nvp("gustSpeed", grass.gustSpeed),
                cereal::make_nvp("gustStrength", grass.gustStrength),
                cereal::make_nvp("swaySpeed", grass.swaySpeed),
                cereal::make_nvp("swayStrength", grass.swayStrength),
                cereal::make_nvp("playerPushRadius", grass.playerPushRadius),
                cereal::make_nvp("playerPushStrength", grass.playerPushStrength),
                cereal::make_nvp("fadeStartRatio", grass.fadeStartRatio));
    }

    template <class Archive>
    void load(Archive& archive, GrassFieldComponent& grass) {
        LoadField(archive, "enabled", grass.enabled);
        LoadField(archive, "bladeCount", grass.bladeCount);
        LoadField(archive, "fieldSize", grass.fieldSize);
        LoadField(archive, "seed", grass.seed);
        LoadField(archive, "farFieldSize", grass.farFieldSize);
        LoadField(archive, "farBladeCount", grass.farBladeCount);
        LoadField(archive, "lodBlendStart", grass.lodBlendStart);
        LoadField(archive, "lodBlendEnd", grass.lodBlendEnd);
        LoadField(archive, "horizonFillStart", grass.horizonFillStart);
        LoadField(archive, "horizonFillEnd", grass.horizonFillEnd);
        LoadField(archive, "species", grass.species);
        LoadField(archive, "bladeWidth", grass.bladeWidth);
        LoadField(archive, "heightVariance", grass.heightVariance);
        LoadField(archive, "groundHeight", grass.groundHeight);
        LoadField(archive, "distantWidthBoost", grass.distantWidthBoost);
        LoadField(archive, "clumpRadiusMin", grass.clumpRadiusMin);
        LoadField(archive, "clumpRadiusMax", grass.clumpRadiusMax);
        LoadField(archive, "clumpSpawnChance", grass.clumpSpawnChance);
        LoadField(archive, "clumpShapeNoise", grass.clumpShapeNoise);
        LoadField(archive, "clumpEdgeSoftness", grass.clumpEdgeSoftness);
        LoadField(archive, "fillerDensity", grass.fillerDensity);
        LoadField(archive, "fillerHeightScale", grass.fillerHeightScale);
        LoadField(archive, "windDirection", grass.windDirection);
        LoadField(archive, "windStrength", grass.windStrength);
        LoadField(archive, "gustWavelength", grass.gustWavelength);
        LoadField(archive, "gustSpeed", grass.gustSpeed);
        LoadField(archive, "gustStrength", grass.gustStrength);
        LoadField(archive, "swaySpeed", grass.swaySpeed);
        LoadField(archive, "swayStrength", grass.swayStrength);
        LoadField(archive, "playerPushRadius", grass.playerPushRadius);
        LoadField(archive, "playerPushStrength", grass.playerPushStrength);
        LoadField(archive, "fadeStartRatio", grass.fadeStartRatio);
    }
}    // namespace CombatAndroid::ECS
