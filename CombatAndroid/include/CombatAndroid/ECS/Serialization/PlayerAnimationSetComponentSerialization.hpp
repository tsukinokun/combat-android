//-------------------------------------------------------------
//! @file   PlayerAnimationSetComponentSerialization.hpp
//! @brief  PlayerAnimationSetComponent（とAttackStep）のcerealシリアライズ定義
//! @note   クリップ（AssetRef＝パス）と連撃の各段・保険タイムアウトだけを保存する。
//!         ステート・タイマー・先行入力などの実行時状態は保存しない。
//!         連撃の段は配列ではなく attackStep0〜2 の名前付きで書く：AssetRefResolverArchiveは
//!         load()を持たないC配列の中を辿れず、段のclipのパス解決が漏れるため
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <cereal/cereal.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void save(Archive& archive, const AttackStep& step) {
        archive(cereal::make_nvp("clip", step.clip),
                cereal::make_nvp("animationIndex", step.animationIndex),
                cereal::make_nvp("startTime", step.startTime),
                cereal::make_nvp("endTime", step.endTime),
                cereal::make_nvp("comboWindowStart", step.comboWindowStart),
                cereal::make_nvp("hitWindowDuration", step.hitWindowDuration),
                cereal::make_nvp("damageMultiplier", step.damageMultiplier),
                cereal::make_nvp("fadeTime", step.fadeTime),
                cereal::make_nvp("playbackSpeed", step.playbackSpeed),
                cereal::make_nvp("inPlace", step.inPlace),
                cereal::make_nvp("areaAttack", step.areaAttack),
                cereal::make_nvp("areaAttackDelay", step.areaAttackDelay));
    }

    template <class Archive>
    void load(Archive& archive, AttackStep& step) {
        LoadField(archive, "clip", step.clip);
        LoadField(archive, "animationIndex", step.animationIndex);
        LoadField(archive, "startTime", step.startTime);
        LoadField(archive, "endTime", step.endTime);
        LoadField(archive, "comboWindowStart", step.comboWindowStart);
        LoadField(archive, "hitWindowDuration", step.hitWindowDuration);
        LoadField(archive, "damageMultiplier", step.damageMultiplier);
        LoadField(archive, "fadeTime", step.fadeTime);
        LoadField(archive, "playbackSpeed", step.playbackSpeed);
        LoadField(archive, "inPlace", step.inPlace);
        LoadField(archive, "areaAttack", step.areaAttack);
        LoadField(archive, "areaAttackDelay", step.areaAttackDelay);
    }

    template <class Archive>
    void save(Archive& archive, const PlayerAnimationSetComponent& set) {
        archive(cereal::make_nvp("idleClip", set.idleClip),
                cereal::make_nvp("runClip", set.runClip),
                cereal::make_nvp("fastRunClip", set.fastRunClip),
                cereal::make_nvp("dodgeClip", set.dodgeClip),
                cereal::make_nvp("deathClip", set.deathClip),
                cereal::make_nvp("attackStep0", set.attackSteps[0]),
                cereal::make_nvp("attackStep1", set.attackSteps[1]),
                cereal::make_nvp("attackStep2", set.attackSteps[2]),
                cereal::make_nvp("attackTimeoutSafety", set.attackTimeoutSafety));
    }

    template <class Archive>
    void load(Archive& archive, PlayerAnimationSetComponent& set) {
        static_assert(PlayerAnimationSetComponent::kAttackComboCount == 3, "attackStep0〜2の名前付き保存は3段を前提にしている");

        LoadField(archive, "idleClip", set.idleClip);
        LoadField(archive, "runClip", set.runClip);
        LoadField(archive, "fastRunClip", set.fastRunClip);
        LoadField(archive, "dodgeClip", set.dodgeClip);
        LoadField(archive, "deathClip", set.deathClip);
        LoadField(archive, "attackStep0", set.attackSteps[0]);
        LoadField(archive, "attackStep1", set.attackSteps[1]);
        LoadField(archive, "attackStep2", set.attackSteps[2]);
        LoadField(archive, "attackTimeoutSafety", set.attackTimeoutSafety);
    }
}    // namespace CombatAndroid::ECS
