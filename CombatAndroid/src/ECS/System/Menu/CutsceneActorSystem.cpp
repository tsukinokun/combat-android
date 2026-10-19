//-------------------------------------------------------------
//! @file   CutsceneActorSystem.cpp
//! @brief  CutsceneActorSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Menu/CutsceneActorSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/Utility/Asset/GamePrefab.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @brief  次の動きへ移り、そのクリップへクロスフェードする（最初の動きだけは即座に切り替える）
        //-------------------------------------------------------------
        void EnterStep(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, CutsceneActorComponent& actor) {
            const bool first = actor.currentStep < 0;
            actor.currentStep = (actor.currentStep + 1) % static_cast<int>(actor.steps.size());
            actor.stepTimer   = 0.0f;

            const CutsceneActorStep& step = actor.steps[static_cast<size_t>(actor.currentStep)];
            auto* controller = registry.try_get<Tsukino::BuiltIn::ECS::AnimationControllerComponent>(entity);
            auto* player     = registry.try_get<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity);
            if(!controller || !player || !step.clip.IsValid())
                return;

            controller->next.clip            = step.clip;
            controller->next.animation_index = step.animationIndex >= 0 ? static_cast<Tsukino::u32>(step.animationIndex) : player->animation_index;
            controller->next.fade_time       = actor.blendTime;
            controller->next.immediate       = first;
            controller->next.is_looping      = step.looping;
            controller->next.clip_start_time = 0.0f;
            controller->next.clip_end_time   = 0.0f;
            controller->next.in_place        = true;    // 前進はTransformで動かすので、クリップの中の移動は殺す
            player->playback_speed           = 1.0f;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief ムービーの役者を1体出す
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnCutsceneActor(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const char* prefabName,
                                            const hlslpp::float3& position, float yaw, float scale, CutsceneActorComponent actor) {
        Tsukino::ECS::Entity entity = InstantiatePrefab(registry, context, prefabName);
        if(entity == entt::null)
            return entt::null;

        auto& transform    = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
        transform.position = position;
        transform.rotation = hlslpp::quaternion::rotation_y(yaw);
        transform.scale    = transform.scale * scale;
        transform.dirty    = true;

        registry.AddComponent<CutsceneActorComponent>(entity) = std::move(actor);
        return entity;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void CutsceneActorSystem::Update(Tsukino::ECS::Registry& registry, float rawDeltaTime) {
        // 初回読み込みで止まったフレームに一気に進まないよう、カットシーンと同じ上限で刻む
        const float deltaTime      = std::min(rawDeltaTime, 0.1f);
        const bool  cutsceneActive = IsCutsceneActive(registry);
        const int  shotIndex      = GetCutsceneShotIndex(registry);

        std::vector<Tsukino::ECS::Entity> finished;

        auto view = registry.View<CutsceneActorComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(entt::entity entity : view) {
            auto& actor     = view.get<CutsceneActorComponent>(entity);
            auto& transform = view.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);

            //-------------------------------------------------------------
            // 片付け：ムービーが終わった（スキップを含む）か、指定のショットまで進んだ
            //-------------------------------------------------------------
            if(!cutsceneActive || (actor.despawnAtShot >= 0 && shotIndex >= actor.despawnAtShot)) {
                finished.push_back(entity);
                continue;
            }
            if(actor.steps.empty())
                continue;

            if(actor.currentStep < 0)
                EnterStep(registry, entity, actor);

            actor.elapsed += deltaTime;
            actor.stepTimer += deltaTime;

            // 台本の動き：向いている方へ進み、上へ跳ぶ（しゃがむ間は待つ）。時間が来たら次の動きへ
            const CutsceneActorStep& step = actor.steps[static_cast<size_t>(actor.currentStep)];
            if((step.forwardSpeed != 0.0f || step.riseSpeed != 0.0f) && actor.stepTimer >= step.moveDelay) {
                const hlslpp::float3 forward = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), transform.rotation);
                transform.position           = transform.position + forward * step.forwardSpeed * deltaTime
                                     + hlslpp::float3(0.0f, step.riseSpeed * deltaTime, 0.0f);
                transform.dirty              = true;
            }
            if(actor.stepTimer >= step.duration)
                EnterStep(registry, entity, actor);

            // 霧に消える：指定の時刻から不透明度を下げる
            if(actor.fadeStart >= 0.0f) {
                if(auto* model = registry.try_get<Tsukino::BuiltIn::ECS::ModelComponent>(entity)) {
                    const float t  = std::clamp((actor.elapsed - actor.fadeStart) / std::max(actor.fadeDuration, 0.01f), 0.0f, 1.0f);
                    model->opacity = 1.0f - t;
                }
            }
        }

        for(Tsukino::ECS::Entity entity : finished)
            registry.QueueDestroy(entity);
    }
}    // namespace CombatAndroid::ECS
