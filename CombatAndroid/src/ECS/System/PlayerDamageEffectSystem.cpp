//-------------------------------------------------------------
//! @file   PlayerDamageEffectSystem.cpp
//! @brief  PlayerDamageEffectSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PlayerDamageEffectSystem.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerDamageEffectComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Core/Window.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct PlayerDamageEffectParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/PlayerDamageEffect.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct PlayerDamageEffectParams {
            //! @brief 画面フラッシュの色（赤）と、被弾直後の最大アルファ
            hlslpp::float4 screenFlashColor = hlslpp::float4(0.9f, 0.05f, 0.05f, 0.35f);
        };

        template <class Archive>
        void load(Archive& archive, PlayerDamageEffectParams& params) {
            LoadField(archive, "screenFlashColor", params.screenFlashColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const PlayerDamageEffectParams& GetParams() {
            static const PlayerDamageEffectParams s_params = LoadSystemParams<PlayerDamageEffectParams>("PlayerDamageEffect");
            return s_params;
        }

        //! @brief 画面フラッシュに使うテクスチャ（WhitePixel.png）の実ピクセルサイズ。
        //!        PlayerHudSystem::kBarTexturePixelSizeと同じ値（4x4の単色テクスチャ）
        constexpr float kFlashTexturePixelSize = 4.0f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief PlayerDamagedEventの購読を開始する
    //-------------------------------------------------------------
    void PlayerDamageEffectSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_damagedConnection = eventBus.Subscribe<PlayerDamagedEvent>([this](const PlayerDamagedEvent& event) { OnPlayerDamaged(event); });
        (void)GetParams();    // 初回の被弾でファイルを読まないよう先に読んでおく
    }

    //-------------------------------------------------------------
    //! @brief 被弾通知のハンドラ
    //-------------------------------------------------------------
    void PlayerDamageEffectSystem::OnPlayerDamaged(const PlayerDamagedEvent& /*event*/) {
        // 現状プレイヤーは1体だけの想定なので、対象を選ばず「次のUpdateで演出を開始する」フラグだけ立てる
        m_pendingTrigger = true;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PlayerDamageEffectSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const PlayerDamageEffectParams& params = GetParams();

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();

        auto view = registry.View<PlayerComponent, PlayerDamageEffectComponent, Tsukino::BuiltIn::ECS::ModelComponent>();

        for(entt::entity entity : view) {
            auto& effect = view.get<PlayerDamageEffectComponent>(entity);
            auto& model  = view.get<Tsukino::BuiltIn::ECS::ModelComponent>(entity);

            if(m_pendingTrigger) {
                effect.blinkTimer       = effect.blinkDuration;
                effect.blinkPhaseTimer = 0.0f;
                effect.screenFlashTimer = effect.screenFlashDuration;
            }

            //-------------------------------------------------------------
            // モデルの点滅。blinkInterval秒おきにModelComponent::visibleをトグルする。
            // 時間切れの瞬間は必ずtrueへ戻し、消えたまま固まらないようにする
            //-------------------------------------------------------------
            if(effect.blinkTimer > 0.0f) {
                effect.blinkTimer -= deltaTime;
                effect.blinkPhaseTimer += deltaTime;

                if(effect.blinkPhaseTimer >= effect.blinkInterval) {
                    effect.blinkPhaseTimer -= effect.blinkInterval;
                    model.visible = !model.visible;
                }

                if(effect.blinkTimer <= 0.0f) {
                    effect.blinkTimer = 0.0f;
                    model.visible      = true;
                }
            }

            //-------------------------------------------------------------
            // 画面フラッシュ。screenFlashDuration秒かけて線形にフェードアウトする。
            // 位置・サイズは毎フレーム画面サイズいっぱいに合わせ直す（ウィンドウリサイズに追従するため。
            // PlayerHudSystem::UpdateBarと同じ「テクスチャピクセルサイズで割ってscaleにする」規約）
            //-------------------------------------------------------------
            if(ctx && ctx->window && effect.screenFlashEntity != entt::null
               && registry.HasComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(effect.screenFlashEntity)
               && registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(effect.screenFlashEntity)) {
                auto& flashSprite    = registry.GetComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(effect.screenFlashEntity);
                auto& flashTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(effect.screenFlashEntity);

                float screenWidth  = static_cast<float>(ctx->window->GetWidth());
                float screenHeight = static_cast<float>(ctx->window->GetHeight());

                flashTransform.position = hlslpp::float3(screenWidth * 0.5f, screenHeight * 0.5f, 0.0f);
                flashTransform.scale =
                    hlslpp::float3(screenWidth / kFlashTexturePixelSize, screenHeight / kFlashTexturePixelSize, 1.0f);
                flashTransform.dirty = true;

                float alpha = 0.0f;
                if(effect.screenFlashTimer > 0.0f) {
                    effect.screenFlashTimer -= deltaTime;
                    float t = std::clamp(effect.screenFlashTimer / effect.screenFlashDuration, 0.0f, 1.0f);
                    alpha    = params.screenFlashColor.w * t;
                }

                flashSprite.tintColor = hlslpp::float4(params.screenFlashColor.x, params.screenFlashColor.y, params.screenFlashColor.z, alpha);
            }
        }

        m_pendingTrigger = false;
    }
}    // namespace CombatAndroid::ECS
