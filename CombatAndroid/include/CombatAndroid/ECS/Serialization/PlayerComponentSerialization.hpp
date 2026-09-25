//-------------------------------------------------------------
//! @file   PlayerComponentSerialization.hpp
//! @brief  PlayerComponentのcerealシリアライズ定義
//! @note   移動・回避・溜め攻撃・レベルアップ後無敵の調整値だけを保存する。
//!         武器のEntityハンドル・入力フラグ・タイマーなどの実行時状態は保存しない
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const PlayerComponent& player) {
        archive(cereal::make_nvp("moveSpeed", player.moveSpeed),
                cereal::make_nvp("turnLerpSpeed", player.turnLerpSpeed),
                cereal::make_nvp("sprintSpeedMultiplier", player.sprintSpeedMultiplier),
                cereal::make_nvp("dodgeSpeed", player.dodgeSpeed),
                cereal::make_nvp("dodgePlaybackSpeed", player.dodgePlaybackSpeed),
                cereal::make_nvp("dodgeInvincibleDuration", player.dodgeInvincibleDuration),
                cereal::make_nvp("dodgeCooldown", player.dodgeCooldown),
                cereal::make_nvp("dodgeTimeoutSafety", player.dodgeTimeoutSafety),
                cereal::make_nvp("levelUpInvincibleDuration", player.levelUpInvincibleDuration),
                cereal::make_nvp("chargeStage2Threshold", player.chargeStage2Threshold),
                cereal::make_nvp("chargeStage3Threshold", player.chargeStage3Threshold),
                cereal::make_nvp("chargeMaxDuration", player.chargeMaxDuration),
                cereal::make_nvp("chargePlaybackSpeed", player.chargePlaybackSpeed),
                cereal::make_nvp("chargeReleasePlaybackSpeedMultiplier", player.chargeReleasePlaybackSpeedMultiplier),
                cereal::make_nvp("chargeDamageMultiplierStage1", player.chargeDamageMultiplierStage1),
                cereal::make_nvp("chargeDamageMultiplierStage2", player.chargeDamageMultiplierStage2),
                cereal::make_nvp("chargeDamageMultiplierStage3", player.chargeDamageMultiplierStage3));
    }

    template <class Archive>
    void load(Archive& archive, PlayerComponent& player) {
        LoadField(archive, "moveSpeed", player.moveSpeed);
        LoadField(archive, "turnLerpSpeed", player.turnLerpSpeed);
        LoadField(archive, "sprintSpeedMultiplier", player.sprintSpeedMultiplier);
        LoadField(archive, "dodgeSpeed", player.dodgeSpeed);
        LoadField(archive, "dodgePlaybackSpeed", player.dodgePlaybackSpeed);
        LoadField(archive, "dodgeInvincibleDuration", player.dodgeInvincibleDuration);
        LoadField(archive, "dodgeCooldown", player.dodgeCooldown);
        LoadField(archive, "dodgeTimeoutSafety", player.dodgeTimeoutSafety);
        LoadField(archive, "levelUpInvincibleDuration", player.levelUpInvincibleDuration);
        LoadField(archive, "chargeStage2Threshold", player.chargeStage2Threshold);
        LoadField(archive, "chargeStage3Threshold", player.chargeStage3Threshold);
        LoadField(archive, "chargeMaxDuration", player.chargeMaxDuration);
        LoadField(archive, "chargePlaybackSpeed", player.chargePlaybackSpeed);
        LoadField(archive, "chargeReleasePlaybackSpeedMultiplier", player.chargeReleasePlaybackSpeedMultiplier);
        LoadField(archive, "chargeDamageMultiplierStage1", player.chargeDamageMultiplierStage1);
        LoadField(archive, "chargeDamageMultiplierStage2", player.chargeDamageMultiplierStage2);
        LoadField(archive, "chargeDamageMultiplierStage3", player.chargeDamageMultiplierStage3);
    }
}    // namespace CombatAndroid::ECS
