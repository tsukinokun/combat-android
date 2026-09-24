//-------------------------------------------------------------
//! @file   ScreenFadeSystem.cpp
//! @brief  ScreenFadeSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/ScreenFadeSystem.hpp>
#include <CombatAndroid/ECS/Component/ScreenFadeComponent.hpp>
#include <CombatAndroid/ECS/Utility/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/WorldTimeContext.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <utility>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 暗転・明転のどちらも同じ長さで行う
        constexpr float kFadeSeconds = 0.4f;

        //! 覆う色（黒）
        const hlslpp::float3 kFadeColor = hlslpp::float3(0.0f, 0.0f, 0.0f);
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void ScreenFadeSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
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

            const float progress = std::clamp(fade.elapsed / kFadeSeconds, 0.0f, 1.0f);

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
                                  hlslpp::float4(kFadeColor, alpha));
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
