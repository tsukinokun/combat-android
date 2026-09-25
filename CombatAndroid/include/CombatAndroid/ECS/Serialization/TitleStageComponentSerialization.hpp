//-------------------------------------------------------------
//! @file   TitleStageComponentSerialization.hpp
//! @brief  TitleStageComponent（タイトルの舞台）のcerealシリアライズ定義
//! @note   カメラの基準位置と、武器ごとの置き場所・抜けたあとの高さ・弾ける時刻・揺れの位相だけを保存する。
//!         エンティティ（カメラ・武器）はTitleSceneが生成後に結ぶ。進行状態は保存しない。
//!         武器は weapon0〜 の名前付きで書く（std::arrayの中の構造体をAssetRefResolverArchiveが辿れないため）
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/TitleStageComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/cereal.hpp>

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const TitleStageWeapon& weapon) {
        archive(cereal::make_nvp("groundPosition", weapon.groundPosition),
                cereal::make_nvp("hoverHeight", weapon.hoverHeight),
                cereal::make_nvp("burstTime", weapon.burstTime),
                cereal::make_nvp("spinPhase", weapon.spinPhase),
                cereal::make_nvp("bobPhase", weapon.bobPhase));
    }

    template <class Archive>
    void load(Archive& archive, TitleStageWeapon& weapon) {
        LoadField(archive, "groundPosition", weapon.groundPosition);
        LoadField(archive, "hoverHeight", weapon.hoverHeight);
        LoadField(archive, "burstTime", weapon.burstTime);
        LoadField(archive, "spinPhase", weapon.spinPhase);
        LoadField(archive, "bobPhase", weapon.bobPhase);
    }

    template <class Archive>
    void save(Archive& archive, const TitleStageComponent& stage) {
        archive(cereal::make_nvp("cameraBasePosition", stage.cameraBasePosition), cereal::make_nvp("cameraLookAt", stage.cameraLookAt));
        for(size_t i = 0; i < stage.weapons.size(); ++i)
            archive(cereal::make_nvp(("weapon" + std::to_string(i)).c_str(), stage.weapons[i]));
    }

    template <class Archive>
    void load(Archive& archive, TitleStageComponent& stage) {
        LoadField(archive, "cameraBasePosition", stage.cameraBasePosition);
        LoadField(archive, "cameraLookAt", stage.cameraLookAt);
        for(size_t i = 0; i < stage.weapons.size(); ++i) {
            const std::string name = "weapon" + std::to_string(i);
            LoadField(archive, name.c_str(), stage.weapons[i]);
        }
    }
}    // namespace CombatAndroid::ECS
