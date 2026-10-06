//-------------------------------------------------------------
//! @file   HitImpactEffectSystem.cpp
//! @brief  HitImpactEffectSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Effect/HitImpactEffectSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/ECS/System/EffectSystem.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/Core/Path.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 再生するエフェクトのパス
        constexpr const char* kAttackImpactEffectPath = "CombatAndroid/Assets/Effect/attackImpact.efkefc";

        //-------------------------------------------------------------
        //! @struct HitImpactEffectParams
        //! @brief  見た目の調整値（Assets/Tables/Systems/HitImpactEffect.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct HitImpactEffectParams {
            //! 命中1回あたりの再生スケール。本作は1ユニット≒1cm規約だがEffekseer側はメートル単位で
            //! 作られるため、単位合わせに100前後の値が要る（WeaponComponent::areaAttackEffectScaleと同じ事情。実機で見ながら調整する）
            float effectScale = 100.0f;
        };

        template <class Archive>
        void load(Archive& archive, HitImpactEffectParams& params) {
            LoadField(archive, "effectScale", params.effectScale);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const HitImpactEffectParams& GetParams() {
            static const HitImpactEffectParams s_params = LoadSystemParams<HitImpactEffectParams>("HitImpactEffect");
            return s_params;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief WeaponHitEventの購読を開始する
    //-------------------------------------------------------------
    void HitImpactEffectSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_hitConnection = eventBus.Subscribe<WeaponHitEvent>([this](const WeaponHitEvent& event) { OnWeaponHit(event); });
        (void)GetParams();    // 初回の命中でファイルを読まないよう先に読んでおく
    }

    //-------------------------------------------------------------
    //! @brief ヒット通知のハンドラ
    //-------------------------------------------------------------
    void HitImpactEffectSystem::OnWeaponHit(const WeaponHitEvent& event) {
        m_pendingHits.push_back(event);
    }

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void HitImpactEffectSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        const HitImpactEffectParams& params = GetParams();

        if(m_pendingHits.empty())
            return;

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->assetManager || !ctx->effectSystem) {
            m_pendingHits.clear();
            return;
        }

        const Tsukino::Core::Path effectPath(kAttackImpactEffectPath);

        //--------------------------------------------------------------
        // エフェクトのハンドルを初回だけ遅延ロードする
        //--------------------------------------------------------------
        if(!m_effectHandle.IsValid())
            m_effectHandle = ctx->assetManager->Load(effectPath);

        for(const WeaponHitEvent& event : m_pendingHits) {
            const float position[3] = {event.contactPosition.x, event.contactPosition.y, event.contactPosition.z};
            ctx->effectSystem->PlayEffect(registry, m_effectHandle, effectPath, position, false, params.effectScale);
        }

        m_pendingHits.clear();
    }
}    // namespace CombatAndroid::ECS
