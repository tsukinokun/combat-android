//-------------------------------------------------------------
//! @file   ProjectileSystem.cpp
//! @brief  ProjectileSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/ProjectileSystem.hpp>

#include <CombatAndroid/ECS/Component/ProjectileComponent.hpp>
#include <CombatAndroid/ECS/Utility/CombatHit.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/ECS/System/PhysicsSystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Physics/SpringBone/SpringBoneMath.hpp>
#ifdef _DEBUG
#include <CombatAndroid/ECS/Utility/CombatDebugDraw.hpp>
#endif

#include <entt/entt.hpp>
#include <hlsl++.h>
#include <algorithm>
#include <cmath>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // JPH::CapsuleShapeはhalfHeight>0を要求する（0はJPH_ASSERTで落ちる）。
        // フレーム落ち等で移動量がほぼ0になったときのために、無視できるほど薄い下限を設ける
        //-------------------------------------------------------------
        constexpr float kMinSweepHalfHeight = 2.0f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void ProjectileSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx      = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>();

        auto view = registry.View<ProjectileComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](entt::entity entity, ProjectileComponent& projectile, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            //-------------------------------------------------------------
            // 直進させる。姿勢（進行方向を向く回転）は発射時に確定しており、
            // EffectSystemがEffectComponent::followRotation経由でエフェクトへ反映する
            //-------------------------------------------------------------
            hlslpp::float3 prevPosition  = transform.position;
            float          stepDistance  = projectile.speed * deltaTime;

            transform.position = prevPosition + projectile.direction * stepDistance;
            transform.dirty    = true;

            projectile.traveledDistance += stepDistance;

            //-------------------------------------------------------------
            // 当たり判定。前フレーム位置→今フレーム位置を芯線とするカプセルでオーバーラップを取る。
            // 弾は毎フレーム大きく進むため、今フレームの位置に球を置くだけでは敵を跨いですり抜ける
            // （武器スイングのサブステップ判定と同じ理屈。CombatSystem参照）。
            // 貫通する弾は当たっても飛び続け、既にヒットさせた敵だけをhitEnemiesで弾く。
            // 貫通しない弾（溜めの浅いもの）は最初にダメージを与えた1体で消滅する
            //-------------------------------------------------------------
            bool           stoppedByHit = false;    //!< 貫通しない弾が敵に当たって止まったか
            hlslpp::float3 sweepSegment = transform.position - prevPosition;
            float          sweepLength  = hlslpp::length(sweepSegment);
            hlslpp::float3 sweepCenter  = (prevPosition + transform.position) * 0.5f;

            if(ctx && ctx->physicsSystem) {
                hlslpp::quaternion sweepRotation = (sweepLength > 1e-4f)
                    ? Tsukino::Physics::QuatFromToRotation(hlslpp::float3(0.0f, 1.0f, 0.0f), sweepSegment / sweepLength)
                    : hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                float halfHeight = std::max(sweepLength * 0.5f, kMinSweepHalfHeight);

                std::vector<entt::entity> overlapping =
                    ctx->physicsSystem->OverlapCapsule(sweepCenter, sweepRotation, projectile.radius, halfHeight);

                for(entt::entity hitEntity : overlapping) {
                    bool landed = ApplyCombatHit(registry, eventBus, projectile.owner, entity, hitEntity, projectile.damage, sweepCenter,
                                                 projectile.hitEnemies, projectile.lifeStealRatio, projectile.lifeStealHealed);

                    // 同じフレームのオーバーラップに複数体が含まれることがあるため、
                    // 貫通しない弾は1体目にダメージが入った時点で残りを見ずに打ち切る
                    if(landed && !projectile.piercing) {
                        stoppedByHit = true;
                        break;
                    }
                }

#ifdef _DEBUG
                // 判定カプセルの可視化。見た目のエフェクトとどれだけずれているかを目視で詰める。
                // 描画コマンドのフラッシュ（Renderer::FlushDebugDraw）はCombatSystemが毎フレーム
                // 積んでおり、実行されるのは全システムの更新が終わった描画時なので、ここで足した線も一緒に出る
                if(ctx->renderer) {
                    DrawWireCapsule(ctx->renderer, sweepCenter, sweepRotation, projectile.radius, halfHeight,
                                    hlslpp::float4(0.2f, 1.0f, 0.6f, 1.0f));
                }
#endif
            }

            //-------------------------------------------------------------
            // 寿命・飛距離のどちらか早い方で破棄する。System内からの破棄は必ずQueueDestroyを使う
            // （即時破棄はイテレータを壊す）。Effekseerの再生ハンドルはEffectComponentの
            // 破棄シグナルがEffectSystem側で確実に回収するため、ここで明示的に止める必要はない
            //-------------------------------------------------------------
            projectile.remainingLifetime -= deltaTime;
            if(stoppedByHit || projectile.remainingLifetime <= 0.0f || projectile.traveledDistance >= projectile.maxDistance) {
                registry.QueueDestroy(entity);
            }
        });
    }
}    // namespace CombatAndroid::ECS
