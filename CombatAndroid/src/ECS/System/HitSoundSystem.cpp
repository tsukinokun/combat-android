//-------------------------------------------------------------
//! @file   HitSoundSystem.cpp
//! @brief  HitSoundSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/HitSoundSystem.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Audio/AudioAsset.hpp>
#include <Tsukino/Audio/AudioManager.hpp>

#include <Tsukino/Core/Path.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 命中1回あたりの再生音量。ダメージ量による強弱は今回は付けない
        constexpr float kHitVolume = 0.8f;

        //-------------------------------------------------------------
        //! @brief ヒットした武器の質感が「鈍器」かどうかを判定する
        //! @param registry [in] エンティティレジストリ
        //! @param weapon   [in] WeaponHitEvent::weapon（ヒットした武器、または斬撃弾のエンティティ）
        //! @return true: 鈍器（ウォーハンマー）向けの音を鳴らす
        //! @note   斬撃弾（ProjectileComponent）はWeaponComponentを持たない。
        //!         斬撃弾を撃てるのはバトルアックスの溜め攻撃だけ（WeaponSpawnDefinition::
        //!         projectileEffectPath参照）なので、WeaponComponentが見つからない場合は
        //!         「刃物」側（false）として扱えば正しい
        //-------------------------------------------------------------
        bool IsBluntWeapon(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity weapon) {
            const auto* weaponComponent = registry.try_get<WeaponComponent>(weapon);
            if(!weaponComponent)
                return false;

            return weaponComponent->weaponId == WeaponId::Warhammer;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief WeaponHitEventの購読を開始する
    //-------------------------------------------------------------
    void HitSoundSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_hitConnection = eventBus.Subscribe<WeaponHitEvent>([this](const WeaponHitEvent& event) { OnWeaponHit(event); });
    }

    //-------------------------------------------------------------
    //! @brief ヒット通知のハンドラ
    //-------------------------------------------------------------
    void HitSoundSystem::OnWeaponHit(const WeaponHitEvent& event) {
        m_pendingHits.push_back(event);
    }

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void HitSoundSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        if(m_pendingHits.empty())
            return;

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->assetManager || !ctx->audioManager) {
            m_pendingHits.clear();
            return;
        }

        //--------------------------------------------------------------
        // ヒット音のハンドルを初回だけ遅延ロードする。
        // .wav は AssetManager::Load が初回アクセス時に自動で .xwb へ変換する
        //--------------------------------------------------------------
        if(!m_bluntSoundHandle.IsValid())
            m_bluntSoundHandle = ctx->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Audio/HitImpactBlunt.wav"));

        if(!m_sharpSoundHandle.IsValid())
            m_sharpSoundHandle = ctx->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Audio/HitImpactSharp.wav"));

        auto bluntAsset = std::dynamic_pointer_cast<Tsukino::Asset::AudioAsset>(ctx->assetManager->Get(m_bluntSoundHandle));
        auto sharpAsset = std::dynamic_pointer_cast<Tsukino::Asset::AudioAsset>(ctx->assetManager->Get(m_sharpSoundHandle));

        for(const WeaponHitEvent& event : m_pendingHits) {
            const bool isBlunt = IsBluntWeapon(registry, event.weapon);
            const auto& asset  = isBlunt ? bluntAsset : sharpAsset;

            if(asset)
                ctx->audioManager->Play(*asset, false, kHitVolume);
        }

        m_pendingHits.clear();
    }
}    // namespace CombatAndroid::ECS
