//-------------------------------------------------------------
//! @file   NightTransitionSystem.cpp
//! @brief  NightTransitionSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/World/NightTransitionSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/World/NightTransitionComponent.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/DirectionalLightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FogComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SkyAtmosphereComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SphereColorGradeComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Renderer/Renderer.hpp>

#include <entt/entt.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct NightTransitionParams
        //! @brief  夜の見た目と切り替えの速さ（Assets/Tables/Systems/NightTransition.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct NightTransitionParams {
            //-------------------------------------------------------------
            // 広がる球（時間は実時間）
            //-------------------------------------------------------------
            float expandSeconds = 3.0f;        //!< 半径が0から maxRadius に届くまで
            float maxRadius     = 4500.0f;     //!< 広がりきった半径（ボスから、カメラの遠方クリップ2000の先まで覆う大きさ）
            float edgeSoftness  = 250.0f;      //!< 境界をなめらかに切り替える幅
            float edgeWidth     = 120.0f;      //!< 境界の光の帯の幅

            hlslpp::float3 edgeColor = hlslpp::float3(0.6f, 0.9f, 2.4f);    //!< 境界の光（HDR）

            //-------------------------------------------------------------
            // 夜の色合い（球の内側）
            //-------------------------------------------------------------
            hlslpp::float3 tint       = hlslpp::float3(0.55f, 0.62f, 0.85f);    //!< 掛ける色（暗く青く。暗さの大半は月の光の弱さで出し、ここは色合いに留める）
            hlslpp::float3 fogColor   = hlslpp::float3(0.015f, 0.022f, 0.06f);  //!< 遠くほど寄せる夜霧の色
            float          fogDensity = 0.0006f;

            //-------------------------------------------------------------
            // 太陽 → 月・空・霧（世界全体の値）。球が広がり始めてから lightDelaySeconds 後に
            // lightBlendSeconds かけて夜の値へ寄せ、寄せきったら環境光を焼き直す。
            // 夜はディレクショナルライトをそのまま月にする（向き・色・強さを替える）。空の太陽の円盤も
            // ライトの向きに出るので、月の向きへ動いて月に見える
            //-------------------------------------------------------------
            float          lightDelaySeconds   = 0.8f;
            float          lightBlendSeconds   = 2.0f;
            float          moonIntensity       = 3.5f;
            hlslpp::float3 moonColor           = hlslpp::float3(0.70f, 0.80f, 1.0f);
            hlslpp::float3 moonDirection       = hlslpp::float3(-0.5f, -0.6f, 0.6f);    //!< 月の光が進む向き（正規化して使う）
            float          nightSkySunIntensity = 6.0f;    //!< 夜空の明るさ。環境光はこの空から焼くので、下げすぎると影が真っ黒になる
            hlslpp::float3 nightFogSunColor    = hlslpp::float3(0.12f, 0.15f, 0.28f);

            float daybreakSeconds = 3.0f;    //!< 夜明けにかける時間
        };

        template <class Archive>
        void load(Archive& archive, NightTransitionParams& params) {
            LoadField(archive, "expandSeconds", params.expandSeconds);
            LoadField(archive, "maxRadius", params.maxRadius);
            LoadField(archive, "edgeSoftness", params.edgeSoftness);
            LoadField(archive, "edgeWidth", params.edgeWidth);
            LoadField(archive, "edgeColor", params.edgeColor);
            LoadField(archive, "tint", params.tint);
            LoadField(archive, "fogColor", params.fogColor);
            LoadField(archive, "fogDensity", params.fogDensity);
            LoadField(archive, "lightDelaySeconds", params.lightDelaySeconds);
            LoadField(archive, "lightBlendSeconds", params.lightBlendSeconds);
            LoadField(archive, "moonIntensity", params.moonIntensity);
            LoadField(archive, "moonColor", params.moonColor);
            LoadField(archive, "moonDirection", params.moonDirection);
            LoadField(archive, "nightSkySunIntensity", params.nightSkySunIntensity);
            LoadField(archive, "nightFogSunColor", params.nightFogSunColor);
            LoadField(archive, "daybreakSeconds", params.daybreakSeconds);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const NightTransitionParams& GetParams() {
            static const NightTransitionParams s_params = LoadSystemParams<NightTransitionParams>("NightTransition");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief  唯一の切り替えの状態を探す
        //-------------------------------------------------------------
        NightTransitionComponent* FindTransition(Tsukino::ECS::Registry& registry) {
            auto view = registry.View<NightTransitionComponent>();
            for(entt::entity entity : view)
                return &view.get<NightTransitionComponent>(entity);
            return nullptr;
        }

        //-------------------------------------------------------------
        //! @brief  0〜1のなめらかな補間（スムーズステップ）
        //-------------------------------------------------------------
        float Smooth(float t) {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        //-------------------------------------------------------------
        //! @brief  昼の太陽・空・霧の値を控える（初めて夜へ入るときに1度だけ）
        //-------------------------------------------------------------
        void CaptureDayValues(Tsukino::ECS::Registry& registry, NightTransitionComponent& transition) {
            if(transition.dayCaptured)
                return;

            auto lightView = registry.View<Tsukino::BuiltIn::ECS::DirectionalLightComponent>();
            for(entt::entity entity : lightView) {
                const auto& light            = lightView.get<Tsukino::BuiltIn::ECS::DirectionalLightComponent>(entity);
                transition.daySunIntensity = light.intensity;
                transition.daySunColor     = light.color;
                transition.daySunDirection = light.direction;
                break;
            }

            auto skyView = registry.View<Tsukino::BuiltIn::ECS::SkyAtmosphereComponent>();
            for(entt::entity entity : skyView) {
                transition.daySkySunIntensity = skyView.get<Tsukino::BuiltIn::ECS::SkyAtmosphereComponent>(entity).sunIntensity;
                break;
            }

            auto fogView = registry.View<Tsukino::BuiltIn::ECS::FogComponent>();
            for(entt::entity entity : fogView) {
                transition.dayFogSunColor = fogView.get<Tsukino::BuiltIn::ECS::FogComponent>(entity).sunColor;
                break;
            }

            transition.dayCaptured = true;
        }

        //-------------------------------------------------------------
        //! @brief  太陽・空・霧を、昼(0)と夜(1)の間の blend の値にする
        //-------------------------------------------------------------
        void ApplyLightBlend(Tsukino::ECS::Registry& registry, const NightTransitionComponent& transition, float blend) {
            const NightTransitionParams& params = GetParams();

            auto lightView = registry.View<Tsukino::BuiltIn::ECS::DirectionalLightComponent>();
            for(entt::entity entity : lightView) {
                auto& light     = lightView.get<Tsukino::BuiltIn::ECS::DirectionalLightComponent>(entity);
                light.intensity = transition.daySunIntensity + (params.moonIntensity - transition.daySunIntensity) * blend;
                light.color     = hlslpp::lerp(transition.daySunColor, params.moonColor, hlslpp::float1(blend));

                // 向きは昼の太陽から月へ回す（正規化した2本の間を補間して正規化し直す）
                const hlslpp::float3 sunDirection  = hlslpp::normalize(transition.daySunDirection);
                const hlslpp::float3 moonDirection = hlslpp::normalize(params.moonDirection);
                light.direction = hlslpp::normalize(hlslpp::lerp(sunDirection, moonDirection, hlslpp::float1(blend)));
                break;
            }

            auto skyView = registry.View<Tsukino::BuiltIn::ECS::SkyAtmosphereComponent>();
            for(entt::entity entity : skyView) {
                auto& sky        = skyView.get<Tsukino::BuiltIn::ECS::SkyAtmosphereComponent>(entity);
                sky.sunIntensity = transition.daySkySunIntensity + (params.nightSkySunIntensity - transition.daySkySunIntensity) * blend;
                break;
            }

            auto fogView = registry.View<Tsukino::BuiltIn::ECS::FogComponent>();
            for(entt::entity entity : fogView) {
                fogView.get<Tsukino::BuiltIn::ECS::FogComponent>(entity).sunColor =
                    hlslpp::lerp(transition.dayFogSunColor, params.nightFogSunColor, hlslpp::float1(blend));
                break;
            }
        }

        //-------------------------------------------------------------
        //! @brief  空が変わったので環境光（空から焼いたIBL）を焼き直す
        //-------------------------------------------------------------
        void RecaptureAmbient(Tsukino::ECS::Registry& registry) {
            if(auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>())
                RequestAmbientRecapture(*ctx);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 環境光（空から焼いたIBL）を次の描画で焼き直させる
    //-------------------------------------------------------------
    void RequestAmbientRecapture(Tsukino::EngineIntegration::EngineContext& context) {
        if(context.renderer)
            context.renderer->GetIBL().RequestRecapture();
    }

    //-------------------------------------------------------------
    //! @brief 夜への切り替えを始める
    //-------------------------------------------------------------
    void RequestNightfall(Tsukino::ECS::Registry& registry, const hlslpp::float3& center) {
        NightTransitionComponent* transition = FindTransition(registry);
        if(!transition)
            return;

        transition->center = center;
        if(transition->state == NightState::Night || transition->state == NightState::Nightfall)
            return;

        CaptureDayValues(registry, *transition);
        transition->state   = NightState::Nightfall;
        transition->elapsed = 0.0f;
    }

    //-------------------------------------------------------------
    //! @brief 夜明けを始める
    //-------------------------------------------------------------
    void RequestDaybreak(Tsukino::ECS::Registry& registry) {
        NightTransitionComponent* transition = FindTransition(registry);
        if(!transition || transition->state == NightState::Day || transition->state == NightState::Daybreak)
            return;

        transition->state   = NightState::Daybreak;
        transition->elapsed = 0.0f;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void NightTransitionSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const NightTransitionParams& params = GetParams();

        const float realDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;

        auto view = registry.View<NightTransitionComponent, Tsukino::BuiltIn::ECS::SphereColorGradeComponent>();
        for(entt::entity entity : view) {
            NightTransitionComponent&                    transition = view.get<NightTransitionComponent>(entity);
            Tsukino::BuiltIn::ECS::SphereColorGradeComponent& grade = view.get<Tsukino::BuiltIn::ECS::SphereColorGradeComponent>(entity);

            if(transition.state == NightState::Day) {
                grade.radius = 0.0f;    // 半径0ならエンジンは描画ごと省く
                continue;
            }

            transition.elapsed += realDeltaTime;

            //-------------------------------------------------------------
            // 夜の見た目（球の中身）は状態によらず同じ
            //-------------------------------------------------------------
            grade.enabled      = true;
            grade.center       = transition.center;
            grade.edgeSoftness = params.edgeSoftness;
            grade.edgeWidth    = params.edgeWidth;
            grade.tint         = params.tint;
            grade.fogColor     = params.fogColor;
            grade.fogDensity   = params.fogDensity;

            switch(transition.state) {
                case NightState::Nightfall: {
                    //-------------------------------------------------------------
                    // 半径はなめらかに立ち上げ、なめらかに止める（境界の光がボスの足元から
                    // カメラの手前を通り過ぎ、奥へ遠ざかっていくのが見える速さにする）
                    //-------------------------------------------------------------
                    const float expandT = std::clamp(transition.elapsed / std::max(params.expandSeconds, 0.01f), 0.0f, 1.0f);
                    const float eased   = Smooth(expandT);
                    grade.radius        = params.maxRadius * eased;
                    grade.strength      = 1.0f;
                    grade.edgeColor     = params.edgeColor * (1.0f - expandT);    // 遠くへ行くほど光を弱める

                    const float lightT = (transition.elapsed - params.lightDelaySeconds) / std::max(params.lightBlendSeconds, 0.01f);
                    transition.lightBlend = Smooth(lightT);
                    ApplyLightBlend(registry, transition, transition.lightBlend);

                    if(expandT >= 1.0f && lightT >= 1.0f) {
                        transition.state   = NightState::Night;
                        transition.elapsed = 0.0f;
                        RecaptureAmbient(registry);
                    }
                    break;
                }
                case NightState::Night:
                    grade.radius    = params.maxRadius;
                    grade.strength  = 1.0f;
                    grade.edgeColor = hlslpp::float3(0.0f, 0.0f, 0.0f);
                    break;
                case NightState::Daybreak: {
                    //-------------------------------------------------------------
                    // 夜明けは球ごと薄めながら、太陽・空・霧を昼へ戻す
                    //-------------------------------------------------------------
                    const float t = std::clamp(transition.elapsed / std::max(params.daybreakSeconds, 0.01f), 0.0f, 1.0f);
                    grade.radius    = params.maxRadius;
                    grade.strength  = 1.0f - Smooth(t);
                    grade.edgeColor = hlslpp::float3(0.0f, 0.0f, 0.0f);

                    const float blend = std::min(transition.lightBlend, 1.0f - Smooth(t));
                    ApplyLightBlend(registry, transition, blend);

                    if(t >= 1.0f) {
                        transition.state      = NightState::Day;
                        transition.elapsed    = 0.0f;
                        transition.lightBlend = 0.0f;
                        grade.radius          = 0.0f;
                        RecaptureAmbient(registry);
                    }
                    break;
                }
                case NightState::Day:
                    break;
            }
        }
    }
}    // namespace CombatAndroid::ECS
