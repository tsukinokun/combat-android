//-------------------------------------------------------------
//! @file   AnimationPlayerComponentSerialization.hpp
//! @brief  AnimationPlayerComponentのcerealシリアライズ定義（エンジン未対応のためゲーム側で持つ）
//! @note   再生の初期設定だけを持つ。current_clip_id（AssetHandle）・elapsed_time・解決済みキャッシュなどは
//!         実行時状態なので保存しない（最初のクリップはスポーンした側が入れる）
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

// 名前空間 : Tsukino::BuiltIn::ECS
namespace Tsukino::BuiltIn::ECS {
    template <class Archive>
    void save(Archive& archive, const AnimationPlayerComponent& player) {
        archive(cereal::make_nvp("animation_index", player.animation_index),
                cereal::make_nvp("playback_speed", player.playback_speed),
                cereal::make_nvp("is_looping", player.is_looping),
                cereal::make_nvp("is_playing", player.is_playing),
                cereal::make_nvp("in_place", player.in_place),
                cereal::make_nvp("root_motion_node_name", player.root_motion_node_name));
    }

    template <class Archive>
    void load(Archive& archive, AnimationPlayerComponent& player) {
        CombatAndroid::ECS::LoadField(archive, "animation_index", player.animation_index);
        CombatAndroid::ECS::LoadField(archive, "playback_speed", player.playback_speed);
        CombatAndroid::ECS::LoadField(archive, "is_looping", player.is_looping);
        CombatAndroid::ECS::LoadField(archive, "is_playing", player.is_playing);
        CombatAndroid::ECS::LoadField(archive, "in_place", player.in_place);
        CombatAndroid::ECS::LoadField(archive, "root_motion_node_name", player.root_motion_node_name);
    }
}    // namespace Tsukino::BuiltIn::ECS
