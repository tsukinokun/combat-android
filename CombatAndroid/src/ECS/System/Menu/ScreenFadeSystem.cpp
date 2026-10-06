//-------------------------------------------------------------
//! @file   ScreenFadeSystem.cpp
//! @brief  ScreenFadeSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Menu/ScreenFadeSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Menu/ScreenFadeComponent.hpp>
#include <CombatAndroid/ECS/Utility/UI/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <utility>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct ScreenFadeParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/ScreenFade.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct ScreenFadeParams {
            //! 暗転・明転のどちらも同じ長さで行う
            float fadeSeconds = 0.4f;

            //! 覆う色（黒）
            hlslpp::float3 fadeColor = hlslpp::float3(0.0f, 0.0f, 0.0f);
        };

        template <class Archive>
        void load(Archive& archive, ScreenFadeParams& params) {
            LoadField(archive, "fadeSeconds", params.fadeSeconds);
            LoadField(archive, "fadeColor", params.fadeColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const ScreenFadeParams& GetParams() {
            static const ScreenFadeParams s_params = LoadSystemParams<ScreenFadeParams>("ScreenFade");
            return s_params;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void ScreenFadeSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const ScreenFadeParams& params = GetParams();

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        // ポーズ中・リザルト中・スキル選択中はdeltaTimeが0（または遅く）なるため、実時間で進める
        const float realDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;

        const float screenWidth  = ctx->window ? static_cast<float>(ctx->window->GetWidth()) : 1700.0f;
        const float screenHeight = ctx->window ? static_cast<float>(ctx->window->GetHeight()) : 1000.0f;

        auto view = registry.View<ScreenFadeComponent>();
        for(entt::entity entity : view) {
            ScreenFadeComponent& fade = view.get<ScreenFadeComponent>(entity);

            fade.elapsed += realDeltaTime;

            const float progress = std::clamp(fade.elapsed / std::max(params.fadeSeconds, 0.01f), 0.0f, 1.0f);

            //-------------------------------------------------------------
            // 濃さ。明転は1→0、暗転は0→1。覆っていない間は板ごと隠して1枚も描かない
            //-------------------------------------------------------------
            float alpha = 0.0f;
            switch(fade.state) {
            case ScreenFadeState::FadingIn:
                alpha = 1.0f - progress;
                if(progress >= 1.0f)
                    fade.state = ScreenFadeState::Idle;
                break;

            case ScreenFadeState::FadingOut:
                alpha = progress;
                break;

            case ScreenFadeState::Idle:
            default:
                break;
            }

            if(fade.panelEntity != entt::null) {
                if(alpha > 0.0f) {
                    StretchSprite(registry, *ctx, fade.panelEntity, screenWidth * 0.5f, screenHeight * 0.5f, screenWidth, screenHeight,
                                  hlslpp::float4(params.fadeColor, alpha));
                } else {
                    HideUiSprite(registry, fade.panelEntity);
                }
            }

            //-------------------------------------------------------------
            // 覆いきったらシーンを切り替える。ChangeSceneは予約するだけで、
            // 実際の切り替えは次フレームの頭（GameSceneManager::Update）で行われるため、
            // 真っ黒な絵がもう1枚出てから次のシーンへ移る
            //-------------------------------------------------------------
            if(fade.state == ScreenFadeState::FadingOut && !fade.handedOff && progress >= 1.0f) {
                fade.handedOff = true;

                if(ctx->gameSceneManager && fade.nextScene)
                    ctx->gameSceneManager->ChangeScene(fade.nextScene());
            }
        }
    }
}    // namespace CombatAndroid::ECS
