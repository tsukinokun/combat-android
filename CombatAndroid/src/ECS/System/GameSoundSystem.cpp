//-------------------------------------------------------------
//! @file   GameSoundSystem.cpp
//! @brief  GameSoundSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/GameSoundSystem.hpp>
#include <CombatAndroid/ECS/Utility/WorldTimeContext.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Audio/AudioAsset.hpp>
#include <Tsukino/Audio/AudioManager.hpp>

#include <Tsukino/Core/Path.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @brief  取得ログの種別から鳴らす音を決める
        //! @param  category [in] ログの種別
        //! @return 鳴らす音。音を割り当てていない種別ならSoundId::Count
        //! @note   ログに出る出来事は「プレイヤーにとって良いことが起きた」瞬間と
        //!         ほぼ一致するので、音の起点としてそのまま使える
        //-------------------------------------------------------------
        [[nodiscard]]
        SoundId ResolveGameLogSound(GameLogCategory category) {
            switch(category) {
            case GameLogCategory::WeaponAcquired: return SoundId::Pickup;
            case GameLogCategory::WeaponLevelUp:  return SoundId::WeaponLevelUp;
            case GameLogCategory::PlayerLevelUp:  return SoundId::LevelUp;
            case GameLogCategory::SkillAcquired:  return SoundId::SkillPick;
            case GameLogCategory::DangerRankUp:   return SoundId::DangerUp;
            case GameLogCategory::WeaponEvolved:  return SoundId::WeaponEvolve;
            case GameLogCategory::EliteAppeared:  return SoundId::DangerUp;
            default:                              return SoundId::Count;
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 各種イベントの購読を開始する
    //-------------------------------------------------------------
    void GameSoundSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_soundConnection   = eventBus.Subscribe<SoundEvent>([this](const SoundEvent& event) { Enqueue(event.id); });
        m_damagedConnection = eventBus.Subscribe<PlayerDamagedEvent>([this](const PlayerDamagedEvent&) { Enqueue(SoundId::PlayerHurt); });

        // ヒットそのものの音はHitSoundSystemが鳴らすので、ここで拾うのは「倒した」瞬間だけ
        m_hitConnection = eventBus.Subscribe<WeaponHitEvent>([this](const WeaponHitEvent& event) {
            if(event.killed)
                Enqueue(SoundId::EnemyDown);
        });

        m_logConnection = eventBus.Subscribe<GameLogEvent>([this](const GameLogEvent& event) {
            const SoundId id = ResolveGameLogSound(event.category);
            if(id != SoundId::Count)
                Enqueue(id);
        });
    }

    //-------------------------------------------------------------
    //! @brief 鳴らす音を予約する
    //-------------------------------------------------------------
    void GameSoundSystem::Enqueue(SoundId id) {
        // ハンドラにはRegistryが渡ってこないため、ここでは積むだけにして
        // 実際の再生はUpdateへ一本化する（HitSoundSystemと同じ作法）
        m_pending.push_back(id);
    }

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void GameSoundSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        //-------------------------------------------------------------
        // 間引きの計測は実時間で行う。停止中（メニュー）もdeltaTimeが0になるだけで
        // 音は鳴るので、ゲーム内時間で数えると停止中に間引きが効かなくなる
        //-------------------------------------------------------------
        const float realDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;

        for(float& elapsed : m_elapsedSinceLastPlay)
            elapsed += realDeltaTime;

        if(m_pending.empty())
            return;

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->assetManager || !ctx->audioManager) {
            m_pending.clear();
            return;
        }

        for(SoundId id : m_pending) {
            const size_t index = static_cast<size_t>(id);
            if(index >= static_cast<size_t>(SoundId::Count))
                continue;

            const SoundTableEntry& entry = GetSoundEntry(id);

            // 同じ音が固まって鳴ると割れるので、最短間隔に満たないものは捨てる
            if(m_elapsedSinceLastPlay[index] < entry.minInterval)
                continue;

            // .wav は AssetManager::Load が初回アクセス時に自動で .xwb へ変換する
            if(!m_handles[index].IsValid())
                m_handles[index] = ctx->assetManager->Load(Tsukino::Core::Path(entry.path));

            auto asset = std::dynamic_pointer_cast<Tsukino::Asset::AudioAsset>(ctx->assetManager->Get(m_handles[index]));
            if(!asset)
                continue;

            ctx->audioManager->Play(*asset, false, entry.volume);
            m_elapsedSinceLastPlay[index] = 0.0f;
        }

        m_pending.clear();
    }
}    // namespace CombatAndroid::ECS
