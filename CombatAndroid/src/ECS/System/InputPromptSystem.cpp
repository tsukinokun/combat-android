//-------------------------------------------------------------
//! @file   InputPromptSystem.cpp
//! @brief  InputPromptSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/InputPromptSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>

#include <CombatAndroid/ECS/Component/InputPromptHudComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/GameOverComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>

#include <CombatAndroid/ECS/Utility/InputPromptWidget.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 配置のチューニング値（画面ピクセル単位。ワールド追従ぶんはワールド単位）
        //-------------------------------------------------------------
        //! 溜めゲージをプレイヤーのどれだけ上に出すか。頭（約170ユニット）より上で、
        //! かつ画面上端に逃げない高さ。1ユニット≒1cm規約
        constexpr float kChargePromptWorldHeight = 250.0f;

        //! スキル選択のカード（幅760を画面中央に置く）の右端から、プロンプト列までの間隔
        constexpr float kSkillPromptCardHalfWidth = 380.0f;
        constexpr float kSkillPromptGapFromCard   = 95.0f;

        constexpr float kSkillPromptUpOffsetY      = -75.0f;    //!< 画面中心から見た[W]のY
        constexpr float kSkillPromptDownOffsetY    = 20.0f;     //!< 同じく[S]のY
        constexpr float kSkillPromptConfirmOffsetY = 165.0f;    //!< 同じく決定[F]のY

        constexpr float kRetryPromptOffsetY = 110.0f;    //!< 画面中心から見たリトライ[SPACE]のY（GAME OVERの下）

        constexpr float kModalPromptScale = 1.15f;    //!< モーダル上のプロンプトは少し大きく見せる

        //-------------------------------------------------------------
        //! @brief  溜め段階からゲージの色を求める（白→青→紫）
        //! @param  stage [in] 溜め段階（1〜3）
        //! @return ゲージの点灯色
        //! @note   PlayerAnimationSystemがリムライトへ入れている色と同じ値にしてあり、
        //!         キャラの発光とゲージの色が同じタイミングで変わる
        //-------------------------------------------------------------
        [[nodiscard]]
        hlslpp::float4 ResolveChargeGaugeColor(int stage) {
            switch(stage) {
            case 3:  return hlslpp::float4(0.65f, 0.15f, 1.0f, 1.0f);    // 紫
            case 2:  return hlslpp::float4(0.25f, 0.55f, 1.0f, 1.0f);    // 青
            default: return hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);      // 白
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void InputPromptSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        (void)deltaTime;

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        const bool skillSelectActive = IsSkillSelectActive(registry);

        const float screenWidth   = ctx->window ? static_cast<float>(ctx->window->GetWidth()) : 1700.0f;
        const float screenHeight  = ctx->window ? static_cast<float>(ctx->window->GetHeight()) : 1000.0f;
        const float screenCenterX = screenWidth * 0.5f;
        const float screenCenterY = screenHeight * 0.5f;

        auto view = registry.View<PlayerComponent, InputPromptHudComponent>();
        view.each([&](entt::entity playerEntity, PlayerComponent& player, InputPromptHudComponent& hud) {
            //-------------------------------------------------------------
            // 拾う：範囲内に対象がいるときだけ、対象の頭上に [F] と上向き矢印と名前を出す。
            // メニュー中はFが決定に取られる（PickupSystemも同じ理由で早期リターンしている）ので消す
            //-------------------------------------------------------------
            const bool canShowPickup = !skillSelectActive && player.pickupTarget != entt::null
                                       && registry.HasComponent<PickupComponent>(player.pickupTarget);

            if(canShowPickup) {
                const PickupComponent& pickup = registry.GetComponent<PickupComponent>(player.pickupTarget);

                InputPromptStyle style;
                style.caption = pickup.displayName;

                ShowInputPromptAtWorld(registry, *ctx, hud.pickupPrompt, player.pickupTarget,
                                       hlslpp::float3(0.0f, pickup.labelHeight, 0.0f), style);
            } else {
                HideInputPrompt(registry, hud.pickupPrompt);
            }

            //-------------------------------------------------------------
            // 溜め攻撃：溜め中だけ、プレイヤーの頭上にマウスの絵と長押しゲージを出す。
            // 進行度は「強制解放までの時間」に対する割合なので、ゲージが一周した瞬間に解放される
            //-------------------------------------------------------------
            const bool isCharging =
                !skillSelectActive && player.isCharging && registry.HasComponent<PlayerAnimationSetComponent>(playerEntity);

            if(isCharging) {
                const PlayerAnimationSetComponent& animSet = registry.GetComponent<PlayerAnimationSetComponent>(playerEntity);

                const float chargeProgress =
                    player.chargeMaxDuration > 0.0f ? std::clamp(animSet.chargeTimer / player.chargeMaxDuration, 0.0f, 1.0f) : 0.0f;

                InputPromptStyle style;
                style.holdProgress = chargeProgress;
                style.accentColor  = ResolveChargeGaugeColor(ResolveChargeStage(animSet.chargeTimer, player));

                ShowInputPromptAtWorld(registry, *ctx, hud.chargePrompt, playerEntity,
                                       hlslpp::float3(0.0f, kChargePromptWorldHeight, 0.0f), style);
            } else {
                HideInputPrompt(registry, hud.chargePrompt);
            }

            //-------------------------------------------------------------
            // スキル選択：カードの右脇に [W] [S]（カーソル移動）と [F]（決定）を縦に並べる
            //-------------------------------------------------------------
            if(skillSelectActive) {
                const float promptX = screenCenterX + kSkillPromptCardHalfWidth + kSkillPromptGapFromCard;

                InputPromptStyle style;
                style.scale = kModalPromptScale;

                ShowInputPromptAtScreen(registry, *ctx, hud.skillUpPrompt, promptX, screenCenterY + kSkillPromptUpOffsetY, style);
                ShowInputPromptAtScreen(registry, *ctx, hud.skillDownPrompt, promptX, screenCenterY + kSkillPromptDownOffsetY, style);
                ShowInputPromptAtScreen(registry, *ctx, hud.skillConfirmPrompt, promptX, screenCenterY + kSkillPromptConfirmOffsetY, style);
            } else {
                HideInputPrompt(registry, hud.skillUpPrompt);
                HideInputPrompt(registry, hud.skillDownPrompt);
                HideInputPrompt(registry, hud.skillConfirmPrompt);
            }

            //-------------------------------------------------------------
            // リトライ：GAME OVERのテキストが出てからだけ [SPACE] を出す。
            // GameOverSystemが表示したフレームは入力を拾わないので、UIもそれに合わせて
            // overlayShownを見る（先にキーだけ出て押しても効かない、という状態を作らない）
            //-------------------------------------------------------------
            const bool showRetry = registry.HasComponent<GameOverComponent>(playerEntity)
                                   && registry.GetComponent<GameOverComponent>(playerEntity).overlayShown;

            if(showRetry) {
                InputPromptStyle style;
                style.scale = kModalPromptScale;

                ShowInputPromptAtScreen(registry, *ctx, hud.retryPrompt, screenCenterX, screenCenterY + kRetryPromptOffsetY, style);
            } else {
                HideInputPrompt(registry, hud.retryPrompt);
            }
        });
    }
}    // namespace CombatAndroid::ECS
