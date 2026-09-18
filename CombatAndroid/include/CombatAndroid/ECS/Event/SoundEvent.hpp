//-------------------------------------------------------------
//! @file   SoundEvent.hpp
//! @brief  効果音を1つ鳴らす通知イベント
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/SoundTable.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct SoundEvent
    //! @brief  「この音を鳴らしてほしい」とだけ伝えるイベント。GameSoundSystemが受けて鳴らす
    //! @note   被弾やレベルアップのように、既に意味のあるイベント（PlayerDamagedEvent /
    //!         GameLogEvent）が飛んでいる場面はそちらをGameSoundSystemが直接購読する。
    //!         このイベントは、回避・メニュー操作のように専用の通知が無い場面のためのもの
    //-------------------------------------------------------------
    struct SoundEvent {
        SoundId id;    //!< 鳴らす音
    };

    //-------------------------------------------------------------
    //! @brief  効果音を鳴らす（SoundEventのPublishの短縮形）
    //! @param  registry [in] ECSレジストリ
    //! @param  id       [in] 鳴らす音
    //! @note   EventBusが取れない場合（シーン外）は何もしない
    //-------------------------------------------------------------
    inline void PlaySound(Tsukino::ECS::Registry& registry, SoundId id) {
        if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
            eventBus->Publish(SoundEvent{id});
    }
}    // namespace CombatAndroid::ECS
