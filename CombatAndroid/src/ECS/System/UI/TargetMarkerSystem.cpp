//-------------------------------------------------------------
//! @file   TargetMarkerSystem.cpp
//! @brief  TargetMarkerSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/TargetMarkerSystem.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/SoftTargetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BossMoveSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <Tsukino/Core/Window.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct TargetMarkerParams
        //! @brief  印の見た目の調整値（Assets/Tables/Systems/TargetMarker.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct TargetMarkerParams {
            hlslpp::float4 candidateColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 0.5f);      //!< 候補（振り向き範囲の外）の色とアルファ
            hlslpp::float4 readyColor     = hlslpp::float4(1.0f, 0.72f, 0.25f, 0.95f);   //!< 振り向き範囲内の色とアルファ
            float          candidateThickness = 2.0f;     //!< 候補のときの線の太さ（ピクセル）
            float          readyThickness     = 3.5f;     //!< 範囲内のときの線の太さ
            float          cornerRatio        = 0.22f;    //!< カギ括弧の腕の長さ（枠の短い辺に対する割合）
            float          cornerMin          = 8.0f;     //!< 腕の長さの下限（ピクセル）
            float          cornerMax          = 40.0f;    //!< 腕の長さの上限
            float          widthRatio         = 0.65f;    //!< 枠の幅（枠の高さに対する割合）
            float          padding            = 0.1f;     //!< 体の外へ広げる余白（枠の高さに対する割合）
            float          minBoxSize         = 28.0f;    //!< 遠くの敵でも枠がこれより小さくならない（ピクセル）
            float          acquireScale       = 1.6f;     //!< ターゲットが変わった瞬間の枠の大きさ（1で動きなし）
            float          acquireDuration    = 0.15f;    //!< そこから元の大きさへ縮むまでの秒数
            float          readyBlendSpeed    = 14.0f;    //!< 薄い印⇔濃い印を切り替える速さ（1秒あたりの寄せの強さ）
            hlslpp::float4 vulnerableColor    = hlslpp::float4(1.0f, 0.82f, 0.2f, 1.0f);    //!< ボスの隙（技の後・ダウン中）の色
            float          vulnerableThickness = 5.0f;    //!< ボスの隙の線の太さ（脈打ちの山）
            float          vulnerablePulseSpeed = 2.5f;   //!< 脈打ちの速さ（1秒あたりの回数）
            float          vulnerableScale    = 1.08f;    //!< 脈打ちの山で枠を広げる倍率
        };

        template <class Archive>
        void load(Archive& archive, TargetMarkerParams& params) {
            LoadField(archive, "candidateColor", params.candidateColor);
            LoadField(archive, "readyColor", params.readyColor);
            LoadField(archive, "candidateThickness", params.candidateThickness);
            LoadField(archive, "readyThickness", params.readyThickness);
            LoadField(archive, "cornerRatio", params.cornerRatio);
            LoadField(archive, "cornerMin", params.cornerMin);
            LoadField(archive, "cornerMax", params.cornerMax);
            LoadField(archive, "widthRatio", params.widthRatio);
            LoadField(archive, "padding", params.padding);
            LoadField(archive, "minBoxSize", params.minBoxSize);
            LoadField(archive, "acquireScale", params.acquireScale);
            LoadField(archive, "acquireDuration", params.acquireDuration);
            LoadField(archive, "readyBlendSpeed", params.readyBlendSpeed);
            LoadField(archive, "vulnerableColor", params.vulnerableColor);
            LoadField(archive, "vulnerableThickness", params.vulnerableThickness);
            LoadField(archive, "vulnerablePulseSpeed", params.vulnerablePulseSpeed);
            LoadField(archive, "vulnerableScale", params.vulnerableScale);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const TargetMarkerParams& GetParams() {
            return GetSystemParams<TargetMarkerParams>("TargetMarker");
        }

        //-------------------------------------------------------------
        //! @brief  ワールド座標を画面ピクセルへ写す
        //! @return 写せたか（カメラの後ろならfalse）
        //-------------------------------------------------------------
        bool ProjectToScreen(const hlslpp::float3& world, const Tsukino::Core::Math::matrix& viewProj, float screenW, float screenH, float& outX,
                             float& outY) {
            const hlslpp::float4 clip = hlslpp::mul(hlslpp::float4(world, 1.0f), viewProj);
            if(clip.w <= 0.0001f)
                return false;
            const float ndcX = clip.x / clip.w;
            const float ndcY = clip.y / clip.w;
            outX             = (ndcX * 0.5f + 0.5f) * screenW;
            outY             = (1.0f - (ndcY * 0.5f + 0.5f)) * screenH;    // NDCのYは上向き、画面のピクセルは下向き
            return true;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void TargetMarkerSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const TargetMarkerParams& params = GetParams();

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->window)
            return;

        if(!m_created) {
            for(Tsukino::ECS::Entity& bar : m_bars)
                bar = CreateUiRectEntity(registry, *ctx, UI::kTargetMarker);
            m_created = true;
        }

        auto hideAll = [&]() {
            for(Tsukino::ECS::Entity bar : m_bars)
                HideUiSprite(registry, bar);
            m_shownTarget = entt::null;
            m_readyBlend  = 0.0f;
        };

        // メニュー・カットシーン中は出さない
        if(IsGameplayFrozen(registry) || IsCutsceneActive(registry)) {
            hideAll();
            return;
        }

        //-------------------------------------------------------------
        // ターゲット（プレイヤーは1人の前提）
        //-------------------------------------------------------------
        Tsukino::ECS::Entity target      = entt::null;
        bool                 assistReady = false;
        {
            auto view = registry.View<PlayerComponent, SoftTargetComponent>();
            for(entt::entity entity : view) {
                const auto& softTarget = view.get<SoftTargetComponent>(entity);
                target                 = softTarget.target;
                assistReady            = softTarget.assistReady;
                break;
            }
        }
        if(target == entt::null || !registry.IsValid(target) || !registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(target)) {
            hideAll();
            return;
        }
        if(const auto* health = registry.try_get<HealthComponent>(target); health && health->isDead) {
            hideAll();
            return;
        }

        //-------------------------------------------------------------
        // 主カメラのViewProjection（EliteIndicatorSystemと同じ手順）
        //-------------------------------------------------------------
        Tsukino::Core::Math::matrix viewProj;
        bool                        hasCamera = false;
        {
            auto cameraView = registry.View<Tsukino::BuiltIn::ECS::CameraComponent>();
            for(auto entity : cameraView) {
                const auto& camera = cameraView.get<Tsukino::BuiltIn::ECS::CameraComponent>(entity);
                if(camera.isPrimary) {
                    viewProj  = camera.viewProjMatrix;
                    hasCamera = true;
                    break;
                }
            }
        }
        if(!hasCamera) {
            hideAll();
            return;
        }

        //-------------------------------------------------------------
        // 敵の体の足元と頭頂を画面へ写して、枠の高さと中心を決める。
        // カプセルは extent.x=半径・extent.y=半分の高さ（CollisionComponent）。カプセルが無い敵は人の背丈で代用する
        //-------------------------------------------------------------
        const auto&    targetTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(target);
        hlslpp::float3 bottom          = targetTransform.position;
        hlslpp::float3 top             = targetTransform.position + hlslpp::float3(0.0f, 180.0f, 0.0f);
        if(const auto* collision = registry.try_get<Tsukino::BuiltIn::ECS::CollisionComponent>(target);
           collision && collision->type == Tsukino::BuiltIn::ECS::ColliderType::Capsule) {
            const float centerY = targetTransform.position.y + collision->offsetPosition.y;
            const float reach   = collision->extent.y + collision->extent.x;
            bottom.y            = centerY - reach;
            top.y               = centerY + reach;
        }

        const float screenW = static_cast<float>(ctx->window->GetWidth());
        const float screenH = static_cast<float>(ctx->window->GetHeight());
        float       bx, by, tx, ty;
        if(!ProjectToScreen(bottom, viewProj, screenW, screenH, bx, by) || !ProjectToScreen(top, viewProj, screenW, screenH, tx, ty)) {
            hideAll();
            return;
        }

        //-------------------------------------------------------------
        // 動き：ターゲットが変わったら吸い付きをやり直し、薄い印⇔濃い印は少しずつ寄せる。
        // 大技のスロー中も同じ速さで動くよう、スロー前の実時間で数える
        //-------------------------------------------------------------
        const float realDeltaTime = registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;
        if(target != m_shownTarget) {
            m_shownTarget  = target;
            m_acquireTimer = 0.0f;
        } else {
            m_acquireTimer += realDeltaTime;
        }
        const float blendT = 1.0f - std::exp(-params.readyBlendSpeed * realDeltaTime);
        m_readyBlend += ((assistReady ? 1.0f : 0.0f) - m_readyBlend) * blendT;

        const float acquireT = (params.acquireDuration > 0.0f) ? std::min(m_acquireTimer / params.acquireDuration, 1.0f) : 1.0f;
        const float eased    = 1.0f - (1.0f - acquireT) * (1.0f - acquireT);
        const float scale    = params.acquireScale + (1.0f - params.acquireScale) * eased;

        //-------------------------------------------------------------
        // 枠の大きさ
        //-------------------------------------------------------------
        const float centerX = (bx + tx) * 0.5f;
        const float centerY = (by + ty) * 0.5f;
        float       boxH    = std::abs(by - ty) * (1.0f + params.padding * 2.0f);
        boxH                = std::max(boxH, params.minBoxSize);
        float boxW          = std::max(boxH * params.widthRatio, params.minBoxSize);
        boxH *= scale;
        boxW *= scale;

        float          thickness = params.candidateThickness + (params.readyThickness - params.candidateThickness) * m_readyBlend;
        hlslpp::float4 color     = params.candidateColor + (params.readyColor - params.candidateColor) * m_readyBlend;

        //-------------------------------------------------------------
        // ボスの隙（技の後・ダウン中）は、文字の代わりに金色の脈打つ太い枠で「今が攻め時」を知らせる
        //-------------------------------------------------------------
        if(const auto* boss = registry.try_get<BossMoveSetComponent>(target);
           boss && (boss->phase == BossActionPhase::Recovery || boss->phase == BossActionPhase::Broken)) {
            const float pulse = 0.5f + 0.5f * std::sin(boss->phaseTimer * params.vulnerablePulseSpeed * 6.2831853f);
            color             = params.vulnerableColor;
            thickness         = params.readyThickness + (params.vulnerableThickness - params.readyThickness) * pulse;
            const float grow  = 1.0f + (params.vulnerableScale - 1.0f) * pulse;
            boxW *= grow;
            boxH *= grow;
        }
        const float          arm       = std::clamp(std::min(boxW, boxH) * params.cornerRatio, params.cornerMin, params.cornerMax);

        //-------------------------------------------------------------
        // 四隅のカギ括弧。隅ごとに「横の棒」と「縦の棒」を、隅から内側へ向けて置く
        //-------------------------------------------------------------
        const float left   = centerX - boxW * 0.5f;
        const float right  = centerX + boxW * 0.5f;
        const float upper  = centerY - boxH * 0.5f;
        const float lower  = centerY + boxH * 0.5f;
        const float cornersX[4] = {left, right, left, right};
        const float cornersY[4] = {upper, upper, lower, lower};
        const float inwardX[4]  = {1.0f, -1.0f, 1.0f, -1.0f};    // 隅から内側へ向かう横の向き
        const float inwardY[4]  = {1.0f, 1.0f, -1.0f, -1.0f};    // 隅から内側へ向かう縦の向き（画面は下が+）

        for(int corner = 0; corner < 4; ++corner) {
            const float cx = cornersX[corner];
            const float cy = cornersY[corner];
            const float ix = inwardX[corner];
            const float iy = inwardY[corner];

            // 横の棒：隅から内側へarmの長さ。太さのぶん内側へ寄せて、縦の棒と角でぴったり重ねる
            StretchSprite(registry, *ctx, m_bars[corner * 2 + 0], cx + ix * arm * 0.5f, cy + iy * thickness * 0.5f, arm, thickness, color);
            // 縦の棒
            StretchSprite(registry, *ctx, m_bars[corner * 2 + 1], cx + ix * thickness * 0.5f, cy + iy * arm * 0.5f, thickness, arm, color);
        }
    }
}    // namespace CombatAndroid::ECS
