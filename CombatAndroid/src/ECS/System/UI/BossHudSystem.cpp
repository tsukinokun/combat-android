//-------------------------------------------------------------
//! @file   BossHudSystem.cpp
//! @brief  BossHudSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/BossHudSystem.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BossMoveSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/BossProgressComponent.hpp>
#include <CombatAndroid/ECS/Component/UI/BossHudComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/System/Progression/BossDirectorSystem.hpp>
#include <CombatAndroid/ECS/Utility/Table/BossMoveTable.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <Tsukino/Core/Window.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct BossHudParams
        //! @brief  見た目の調整値（Assets/Tables/Systems/BossHud.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct BossHudParams {
            float barWidth        = 560.0f;    //!< HPバー・体勢ゲージの幅（ピクセル）
            float hpBarHeight     = 14.0f;     //!< HPバーの高さ
            float gaugeHeight     = 7.0f;      //!< 体勢ゲージの高さ
            float gaugeGapY       = 6.0f;      //!< HPバーと体勢ゲージの間
            float bottomMargin    = 64.0f;     //!< 画面の下端から体勢ゲージの下端まで
            float nameGapY        = 16.0f;     //!< HPバーの上端から名前の中心まで
            float frameThickness  = 3.0f;      //!< 体勢ゲージの枠の太さ（ボスの隙の間、脈打ちの山でこの倍まで太る）

            hlslpp::float4 backColor       = hlslpp::float4(0.08f, 0.08f, 0.1f, 0.85f);    //!< 背景の色
            hlslpp::float4 hpColor         = hlslpp::float4(0.82f, 0.12f, 0.16f, 1.0f);    //!< HPの残量の色
            hlslpp::float4 trailColor      = hlslpp::float4(1.0f, 0.95f, 0.85f, 0.9f);     //!< HPの「減った分」の帯の色
            hlslpp::float4 gaugeLowColor   = hlslpp::float4(0.85f, 0.85f, 0.85f, 1.0f);    //!< 体勢ゲージが少ないときの色
            hlslpp::float4 gaugeHighColor  = hlslpp::float4(1.0f, 0.55f, 0.1f, 1.0f);      //!< 体勢ゲージが満タン近くの色
            hlslpp::float4 downColor       = hlslpp::float4(1.0f, 0.85f, 0.3f, 1.0f);      //!< ダウン中のゲージの色（点滅する）
            hlslpp::float4 frameIdleColor  = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.6f);       //!< 普段の枠の色
            hlslpp::float4 frameOpenColor  = hlslpp::float4(1.0f, 0.82f, 0.2f, 1.0f);      //!< ボスの隙の間の枠の色
            hlslpp::float4 nameColor       = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);       //!< 名前の色

            float trailHoldSeconds = 0.35f;    //!< HPが減ってから帯が縮み始めるまで（秒）
            float trailShrinkSpeed = 1.2f;     //!< 帯が縮む速さ（満タン幅に対する割合/秒）
            float pulseSpeed       = 2.5f;     //!< 枠の脈打ちの速さ（1秒あたりの回数）
            float downBlinkSpeed   = 6.0f;     //!< ダウン中のゲージの点滅の速さ（1秒あたりの回数）
        };

        template <class Archive>
        void load(Archive& archive, BossHudParams& params) {
            LoadField(archive, "barWidth", params.barWidth);
            LoadField(archive, "hpBarHeight", params.hpBarHeight);
            LoadField(archive, "gaugeHeight", params.gaugeHeight);
            LoadField(archive, "gaugeGapY", params.gaugeGapY);
            LoadField(archive, "bottomMargin", params.bottomMargin);
            LoadField(archive, "nameGapY", params.nameGapY);
            LoadField(archive, "frameThickness", params.frameThickness);
            LoadField(archive, "backColor", params.backColor);
            LoadField(archive, "hpColor", params.hpColor);
            LoadField(archive, "trailColor", params.trailColor);
            LoadField(archive, "gaugeLowColor", params.gaugeLowColor);
            LoadField(archive, "gaugeHighColor", params.gaugeHighColor);
            LoadField(archive, "downColor", params.downColor);
            LoadField(archive, "frameIdleColor", params.frameIdleColor);
            LoadField(archive, "frameOpenColor", params.frameOpenColor);
            LoadField(archive, "nameColor", params.nameColor);
            LoadField(archive, "trailHoldSeconds", params.trailHoldSeconds);
            LoadField(archive, "trailShrinkSpeed", params.trailShrinkSpeed);
            LoadField(archive, "pulseSpeed", params.pulseSpeed);
            LoadField(archive, "downBlinkSpeed", params.downBlinkSpeed);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const BossHudParams& GetParams() {
            static const BossHudParams s_params = LoadSystemParams<BossHudParams>("BossHud");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief  左端を固定して、割合ぶんの長さの横棒を置く
        //-------------------------------------------------------------
        void PlaceBar(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& ctx, Tsukino::ECS::Entity entity, float leftX,
                      float centerY, float width, float height, float ratio, const hlslpp::float4& color) {
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            if(ratio <= 0.0f) {
                HideUiSprite(registry, entity);
                return;
            }
            StretchSprite(registry, ctx, entity, leftX + width * ratio * 0.5f, centerY, width * ratio, height, color);
        }

        //-------------------------------------------------------------
        //! @brief  部品を全部隠す
        //-------------------------------------------------------------
        void HideAll(Tsukino::ECS::Registry& registry, const BossHudComponent& hud) {
            for(Tsukino::ECS::Entity entity : {hud.hpBackEntity, hud.hpTrailEntity, hud.hpFillEntity, hud.gaugeFrameEntity, hud.gaugeBackEntity,
                                               hud.gaugeFillEntity})
                HideUiSprite(registry, entity);
            HideUiText(registry, hud.nameEntity);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void BossHudSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const BossHudParams& params = GetParams();

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->window)
            return;

        //-------------------------------------------------------------
        // 部品は初回にプレイヤー（ボスの進行状態を持つエンティティ）へ付ける
        //-------------------------------------------------------------
        Tsukino::ECS::Entity         playerEntity = entt::null;
        const BossProgressComponent* progress     = nullptr;
        {
            auto view = registry.View<PlayerComponent, BossProgressComponent>();
            for(entt::entity entity : view) {
                playerEntity = entity;
                progress     = &view.get<BossProgressComponent>(entity);
                break;
            }
        }
        if(playerEntity == entt::null)
            return;

        if(!registry.HasComponent<BossHudComponent>(playerEntity)) {
            // 部品を作るとComponentの格納先が動き得るので、全部作ってから付ける
            const Tsukino::ECS::Entity hpBack     = CreateUiRectEntity(registry, *ctx, UI::kBossHudBack);
            const Tsukino::ECS::Entity hpTrail    = CreateUiRectEntity(registry, *ctx, UI::kBossHudTrail);
            const Tsukino::ECS::Entity hpFill     = CreateUiRectEntity(registry, *ctx, UI::kBossHudFill);
            const Tsukino::ECS::Entity gaugeFrame = CreateUiRectEntity(registry, *ctx, UI::kBossHudFrame);
            const Tsukino::ECS::Entity gaugeBack  = CreateUiRectEntity(registry, *ctx, UI::kBossHudBack);
            const Tsukino::ECS::Entity gaugeFill  = CreateUiRectEntity(registry, *ctx, UI::kBossHudFill);
            const Tsukino::ECS::Entity name       = CreateUiTextEntity(registry, UI::kBossHudText, UiTextAlign::Center, UiTextFont::Heading);

            BossHudComponent& created = registry.AddComponent<BossHudComponent>(playerEntity);
            created.hpBackEntity      = hpBack;
            created.hpTrailEntity     = hpTrail;
            created.hpFillEntity      = hpFill;
            created.gaugeFrameEntity  = gaugeFrame;
            created.gaugeBackEntity   = gaugeBack;
            created.gaugeFillEntity   = gaugeFill;
            created.nameEntity        = name;

            // 進行状態の参照も、エンティティを作った後に取り直す
            progress = &registry.GetComponent<BossProgressComponent>(playerEntity);
        }
        BossHudComponent& hud = registry.GetComponent<BossHudComponent>(playerEntity);

        //-------------------------------------------------------------
        // ボス戦の間だけ出す。メニュー・カットシーン（出現の演出）の間は隠す
        //-------------------------------------------------------------
        const Tsukino::ECS::Entity boss = progress ? progress->bossEntity : entt::null;
        const bool show = IsBossBattleActive(registry) && !IsGameplayFrozen(registry) && !IsCutsceneActive(registry) && boss != entt::null
                          && registry.IsValid(boss) && registry.HasComponent<HealthComponent>(boss);
        if(!show) {
            HideAll(registry, hud);
            hud.shownBoss = entt::null;
            return;
        }

        const HealthComponent&      health  = registry.GetComponent<HealthComponent>(boss);
        const BossMoveSetComponent* moveSet = registry.try_get<BossMoveSetComponent>(boss);
        const float hpRatio = health.maxHealth > 0.0f ? std::clamp(health.currentHealth / health.maxHealth, 0.0f, 1.0f) : 0.0f;

        if(hud.shownBoss != boss) {
            hud.shownBoss      = boss;
            hud.trailRatio     = hpRatio;
            hud.lastHpRatio    = hpRatio;
            hud.trailHoldTimer = 0.0f;
        }
        hud.pulseTime += deltaTime;

        //-------------------------------------------------------------
        // HPの「減った分」の帯：減ってから少し待って縮む（連撃の間は待ち直す）
        //-------------------------------------------------------------
        if(hpRatio < hud.lastHpRatio - 1e-4f)
            hud.trailHoldTimer = params.trailHoldSeconds;
        hud.lastHpRatio = hpRatio;
        if(hud.trailHoldTimer > 0.0f)
            hud.trailHoldTimer = std::max(hud.trailHoldTimer - deltaTime, 0.0f);
        else
            hud.trailRatio = std::max(hpRatio, hud.trailRatio - params.trailShrinkSpeed * deltaTime);
        hud.trailRatio = std::max(hud.trailRatio, hpRatio);

        //-------------------------------------------------------------
        // 配置：画面下中央に、上から名前・HPバー・体勢ゲージ
        //-------------------------------------------------------------
        const float screenW     = static_cast<float>(ctx->window->GetWidth());
        const float screenH     = static_cast<float>(ctx->window->GetHeight());
        const float leftX       = (screenW - params.barWidth) * 0.5f;
        const float gaugeCenter = screenH - params.bottomMargin - params.gaugeHeight * 0.5f;
        const float hpCenter    = gaugeCenter - params.gaugeHeight * 0.5f - params.gaugeGapY - params.hpBarHeight * 0.5f;
        const float nameY       = hpCenter - params.hpBarHeight * 0.5f - params.nameGapY;

        PlaceUiText(registry, hud.nameEntity, screenW * 0.5f, nameY, GetUiTextScale(UiTextSize::Body), progress->announceTitle, params.nameColor);

        StretchSprite(registry, *ctx, hud.hpBackEntity, screenW * 0.5f, hpCenter, params.barWidth, params.hpBarHeight, params.backColor);
        PlaceBar(registry, *ctx, hud.hpTrailEntity, leftX, hpCenter, params.barWidth, params.hpBarHeight, hud.trailRatio, params.trailColor);
        PlaceBar(registry, *ctx, hud.hpFillEntity, leftX, hpCenter, params.barWidth, params.hpBarHeight, hpRatio, params.hpColor);

        //-------------------------------------------------------------
        // 体勢ゲージ。ブレイクを持たないボスは出さない。
        // ダウン中は満タンから残り時間に合わせて減らし、点滅させる
        //-------------------------------------------------------------
        const bool hasBreak = moveSet && moveSet->breakGaugeMax > 0.0f;
        if(!hasBreak) {
            HideUiSprite(registry, hud.gaugeFrameEntity);
            HideUiSprite(registry, hud.gaugeBackEntity);
            HideUiSprite(registry, hud.gaugeFillEntity);
            return;
        }

        const bool  broken     = moveSet->phase == BossActionPhase::Broken;
        const bool  opening    = broken || moveSet->phase == BossActionPhase::Recovery;
        float       gaugeRatio = moveSet->breakGauge / moveSet->breakGaugeMax;
        hlslpp::float4 gaugeColor = params.gaugeLowColor + (params.gaugeHighColor - params.gaugeLowColor) * gaugeRatio;
        if(broken) {
            const float downSeconds = std::max(GetBossMoveSet(moveSet->key).breakDef.downSeconds, 0.01f);
            gaugeRatio              = 1.0f - std::clamp(moveSet->phaseTimer / downSeconds, 0.0f, 1.0f);
            const float blink       = 0.5f + 0.5f * std::sin(hud.pulseTime * params.downBlinkSpeed * 6.2831853f);
            gaugeColor              = params.downColor;
            gaugeColor.w            = 0.55f + 0.45f * blink;
        }

        // 枠：ボスの隙の間は金色に脈打って太る（文字の代わりに「今が攻め時」を伝える）
        const float pulse     = opening ? 0.5f + 0.5f * std::sin(hud.pulseTime * params.pulseSpeed * 6.2831853f) : 0.0f;
        const float thickness = params.frameThickness * (1.0f + pulse);
        StretchSprite(registry, *ctx, hud.gaugeFrameEntity, screenW * 0.5f, gaugeCenter, params.barWidth + thickness * 2.0f,
                      params.gaugeHeight + thickness * 2.0f, opening ? params.frameOpenColor : params.frameIdleColor);
        StretchSprite(registry, *ctx, hud.gaugeBackEntity, screenW * 0.5f, gaugeCenter, params.barWidth, params.gaugeHeight, params.backColor);
        PlaceBar(registry, *ctx, hud.gaugeFillEntity, leftX, gaugeCenter, params.barWidth, params.gaugeHeight, gaugeRatio, gaugeColor);
    }
}    // namespace CombatAndroid::ECS
