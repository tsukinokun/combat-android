//-------------------------------------------------------------
//! @file   TpsCameraComponentSerialization.hpp
//! @brief  TpsCameraComponentのcerealシリアライズ定義
//! @note   カメラの調整値（距離・ばね・被弾の揺れ・大技のズーム・マウス旋回・地面の下限）だけを保存する。
//!         target（Entity）・ばねの位置/速度・ズームの進行・キャプチャ状態などの実行時状態は保存しない
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const TpsCameraComponent& camera) {
        archive(cereal::make_nvp("distance", camera.distance),
                cereal::make_nvp("height", camera.height),
                cereal::make_nvp("lookHeight", camera.lookHeight),
                cereal::make_nvp("followSpringFrequency", camera.followSpringFrequency),
                cereal::make_nvp("followSpringDamping", camera.followSpringDamping),
                cereal::make_nvp("followSpringResetDistance", camera.followSpringResetDistance),
                cereal::make_nvp("shakeFrequency", camera.shakeFrequency),
                cereal::make_nvp("shakeDamping", camera.shakeDamping),
                cereal::make_nvp("shakeImpulse", camera.shakeImpulse),
                cereal::make_nvp("shakeReferenceDamage", camera.shakeReferenceDamage),
                cereal::make_nvp("shakeMinScale", camera.shakeMinScale),
                cereal::make_nvp("shakeMaxScale", camera.shakeMaxScale),
                cereal::make_nvp("zoomDistanceScale", camera.zoomDistanceScale),
                cereal::make_nvp("zoomFovScale", camera.zoomFovScale),
                cereal::make_nvp("zoomHoldAfterImpact", camera.zoomHoldAfterImpact),
                cereal::make_nvp("zoomInFrequency", camera.zoomInFrequency),
                cereal::make_nvp("zoomOutFrequency", camera.zoomOutFrequency),
                cereal::make_nvp("yaw", camera.yaw),
                cereal::make_nvp("pitch", camera.pitch),
                cereal::make_nvp("mouseSensitivity", camera.mouseSensitivity),
                cereal::make_nvp("minPitch", camera.minPitch),
                cereal::make_nvp("maxPitch", camera.maxPitch),
                cereal::make_nvp("groundHeight", camera.groundHeight),
                cereal::make_nvp("minHeightAboveGround", camera.minHeightAboveGround));
    }

    template <class Archive>
    void load(Archive& archive, TpsCameraComponent& camera) {
        LoadField(archive, "distance", camera.distance);
        LoadField(archive, "height", camera.height);
        LoadField(archive, "lookHeight", camera.lookHeight);
        LoadField(archive, "followSpringFrequency", camera.followSpringFrequency);
        LoadField(archive, "followSpringDamping", camera.followSpringDamping);
        LoadField(archive, "followSpringResetDistance", camera.followSpringResetDistance);
        LoadField(archive, "shakeFrequency", camera.shakeFrequency);
        LoadField(archive, "shakeDamping", camera.shakeDamping);
        LoadField(archive, "shakeImpulse", camera.shakeImpulse);
        LoadField(archive, "shakeReferenceDamage", camera.shakeReferenceDamage);
        LoadField(archive, "shakeMinScale", camera.shakeMinScale);
        LoadField(archive, "shakeMaxScale", camera.shakeMaxScale);
        LoadField(archive, "zoomDistanceScale", camera.zoomDistanceScale);
        LoadField(archive, "zoomFovScale", camera.zoomFovScale);
        LoadField(archive, "zoomHoldAfterImpact", camera.zoomHoldAfterImpact);
        LoadField(archive, "zoomInFrequency", camera.zoomInFrequency);
        LoadField(archive, "zoomOutFrequency", camera.zoomOutFrequency);
        LoadField(archive, "yaw", camera.yaw);
        LoadField(archive, "pitch", camera.pitch);
        LoadField(archive, "mouseSensitivity", camera.mouseSensitivity);
        LoadField(archive, "minPitch", camera.minPitch);
        LoadField(archive, "maxPitch", camera.maxPitch);
        LoadField(archive, "groundHeight", camera.groundHeight);
        LoadField(archive, "minHeightAboveGround", camera.minHeightAboveGround);
    }
}    // namespace CombatAndroid::ECS
