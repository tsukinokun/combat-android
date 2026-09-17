//-------------------------------------------------------------
//! @file   ChargeSoundSystem.cpp
//! @brief  ChargeSoundSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/ChargeSoundSystem.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Audio/AudioAsset.hpp>
#include <Tsukino/Audio/AudioManager.hpp>

#include <Tsukino/Core/Path.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 発射の「ドン」の音量。溜めの締めくくりなので大きめに出す
        constexpr float kFireVolume = 0.9f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief PlayerChargeReleasedEventの購読を開始する
    //-------------------------------------------------------------
    void ChargeSoundSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_releasedConnection =
            eventBus.Subscribe<PlayerChargeReleasedEvent>([this](const PlayerChargeReleasedEvent& event) { OnChargeReleased(event); });
    }

    //-------------------------------------------------------------
    //! @brief 解放通知のハンドラ
    //-------------------------------------------------------------
    void ChargeSoundSystem::OnChargeReleased(const PlayerChargeReleasedEvent& event) {
        // 遅延が無い場合も次のUpdateで鳴らせるよう、必ず正の残り時間にしておく
        m_fireTimer = std::max(event.fireDelay, 1e-6f);
    }

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void ChargeSoundSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        // 発射待ちでなければ何もしない（大半のフレームはこちら）
        if(m_fireTimer <= 0.0f)
            return;

        //--------------------------------------------------------------
        // 斬撃弾が飛び出すのと同じ秒数を数え、0になったらドンを鳴らす。
        // deltaTimeはCombatSystemが斬撃弾のタイマーを進めるのと同じワールド時間なので、
        // 大技のスロー中でも音と弾がずれない
        //--------------------------------------------------------------
        m_fireTimer -= deltaTime;
        if(m_fireTimer > 0.0f)
            return;

        m_fireTimer = 0.0f;

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->assetManager || !ctx->audioManager)
            return;

        // .wav は AssetManager::Load が初回アクセス時に自動で .xwb へ変換する
        if(!m_fireSoundHandle.IsValid())
            m_fireSoundHandle = ctx->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Audio/ChargeFire.wav"));

        auto fireAsset = std::dynamic_pointer_cast<Tsukino::Asset::AudioAsset>(ctx->assetManager->Get(m_fireSoundHandle));
        if(fireAsset)
            ctx->audioManager->Play(*fireAsset, false, kFireVolume);
    }
}    // namespace CombatAndroid::ECS
