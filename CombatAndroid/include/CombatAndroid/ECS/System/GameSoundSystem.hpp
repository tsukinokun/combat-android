//-------------------------------------------------------------
//! @file   GameSoundSystem.hpp
//! @brief  GameSoundSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/GameLogEvent.hpp>
#include <CombatAndroid/ECS/Event/PlayerDamagedEvent.hpp>
#include <CombatAndroid/ECS/Event/SoundEvent.hpp>
#include <CombatAndroid/ECS/Event/WeaponHitEvent.hpp>
#include <CombatAndroid/ECS/Utility/SoundTable.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>
#include <Tsukino/Engine/Asset/AssetHandle.hpp>

#include <array>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  GameSoundSystem
    //! @brief  ゲーム全体の効果音を鳴らすシステム。SoundEventを購読するほか、
    //!         既に飛んでいるイベント（被弾・ヒット・取得ログ）からも鳴らし分ける
    //! @note   骨格はHitSoundSystemと同じ（イベントをキューへ積み、Updateで
    //!         AudioManagerを直接呼ぶ）。鳴らす音の一覧と音量はSoundTableにある。
    //!         ヒット音（HitSoundSystem）と溜め攻撃の発射音（ChargeSoundSystem）は、
    //!         武器の質感や発射の遅延といった固有の判断を持つため別のSystemのまま
    //-------------------------------------------------------------
    class GameSoundSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（最短間隔の計測に使う）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief 各種イベントの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除は各ScopedConnectionのデストラクタに任せてよい
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief 鳴らす音を予約する（イベントハンドラから呼ぶ）
        //! @param id [in] 鳴らす音
        //-------------------------------------------------------------
        void Enqueue(SoundId id);

        std::vector<SoundId> m_pending;    //!< 次のUpdateで鳴らす音

        //! 音ごとの「最後に鳴らしてからの経過秒」。SoundTableEntry::minIntervalと比べて重なりを間引く
        std::array<float, static_cast<size_t>(SoundId::Count)> m_elapsedSinceLastPlay{};

        //! 音ごとのアセットハンドル。初回Updateでまとめて遅延ロードする
        std::array<Tsukino::Asset::AssetHandle, static_cast<size_t>(SoundId::Count)> m_handles;

        Tsukino::ECS::ScopedConnection m_soundConnection;      //!< SoundEventの購読
        Tsukino::ECS::ScopedConnection m_damagedConnection;    //!< PlayerDamagedEventの購読
        Tsukino::ECS::ScopedConnection m_hitConnection;        //!< WeaponHitEventの購読（撃破を拾う）
        Tsukino::ECS::ScopedConnection m_logConnection;        //!< GameLogEventの購読（取得・レベルアップ等）
    };
}    // namespace CombatAndroid::ECS
