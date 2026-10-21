//-------------------------------------------------------------
//! @file   CutsceneSystem.cpp
//! @brief  CutsceneSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>

#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/World/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Utility/UI/GameMenu.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>
#include <CombatAndroid/ECS/Utility/Input/GameInput.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Core/Math/Spring.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        namespace Spring = Tsukino::Core::Math::Spring;    // 減衰ばね（エンジンの Spring.hpp）

        //! レターボックス（上下の黒帯）の高さ。画面の高さに対する比率
        constexpr float kLetterboxHeightRatio = 0.09f;

        //! レターボックスの退場アニメーションにかける秒数（実時間）
        constexpr float kLetterboxRetractDuration = 0.35f;

        //! 1フレームで進めてよいショットの時間の上限（秒）。役者のモデルの初回読み込みなどで
        //! 数秒止まったフレームがあっても、その間にショットが飛ばされないようにする
        constexpr float kMaxShotStep = 0.1f;

        const hlslpp::float4      kLetterboxColor(0.0f, 0.0f, 0.0f, 1.0f);
        const hlslpp::float4      kSkipHintColor(0.85f, 0.85f, 0.85f, 1.0f);

        //-------------------------------------------------------------
        //! @brief  唯一のCutsceneComponent（プレイヤーエンティティに付いている）を探す
        //-------------------------------------------------------------
        CutsceneComponent* FindCutscene(Tsukino::ECS::Registry& registry) {
            auto view = registry.View<CutsceneComponent>();
            for(entt::entity entity : view)
                return &view.get<CutsceneComponent>(entity);
            return nullptr;
        }

        //-------------------------------------------------------------
        //! @brief  再生を止める。スキップ案内は即座に隠すが、上下の帯は
        //!         retractingLetterboxを立てて退場アニメーションへ引き継ぐ
        //-------------------------------------------------------------
        void StopCutscene(Tsukino::ECS::Registry& registry, CutsceneComponent& cutscene) {
            //-------------------------------------------------------------
            // TPSカメラへ引き継ぐ。終わった瞬間のカメラの位置をばねの初期位置にし、旋回の向きを
            // 「プレイヤーから今のカメラへの向き」に合わせる（反対側へ大回りしないように）。
            // 注視点と画角はTpsCameraSystemが handoffDuration かけて通常へ寄せる
            //-------------------------------------------------------------
            auto cameraView = registry.View<TpsCameraComponent, Tsukino::BuiltIn::ECS::TransformComponent, Tsukino::BuiltIn::ECS::CameraComponent>();
            for(entt::entity cameraEntity : cameraView) {
                auto&       tpsCamera = cameraView.get<TpsCameraComponent>(cameraEntity);
                const auto& transform = cameraView.get<Tsukino::BuiltIn::ECS::TransformComponent>(cameraEntity);
                const auto& camera    = cameraView.get<Tsukino::BuiltIn::ECS::CameraComponent>(cameraEntity);

                tpsCamera.followSpringPosition = transform.position;
                tpsCamera.followSpringVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);
                tpsCamera.hasFollowSpringState = true;
                tpsCamera.handoffLookAt        = camera.lookAtTarget;
                tpsCamera.handoffFov           = camera.fov;
                tpsCamera.handoffTimer         = tpsCamera.handoffDuration;

                if(tpsCamera.target != entt::null && registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(tpsCamera.target)) {
                    const hlslpp::float3 offset = transform.position - registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(tpsCamera.target).position;
                    if(static_cast<float>(offset.x) * static_cast<float>(offset.x) + static_cast<float>(offset.z) * static_cast<float>(offset.z) > 1.0f)
                        tpsCamera.yaw = std::atan2(static_cast<float>(offset.x), -static_cast<float>(offset.z));
                }
                break;    // TPSカメラは1体だけの想定
            }

            cutscene.playing          = false;
            cutscene.liveWorld        = false;
            cutscene.currentShotIndex = -1;
            cutscene.shotTimer        = 0.0f;
            cutscene.shots.clear();

            HideUiText(registry, cutscene.skipHintTextEntity);

            cutscene.retractingLetterbox   = true;
            cutscene.letterboxRetractTimer = 0.0f;
        }

        //-------------------------------------------------------------
        //! @brief  レターボックスの退場アニメーションを1フレーム分進める。
        //!         上帯は画面上端、下帯は画面下端に外側の縁を固定したまま、
        //!         それぞれの端へ向かって縮んで消える
        //-------------------------------------------------------------
        void UpdateLetterboxRetraction(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& ctx,
                                       CutsceneComponent& cutscene, float realDeltaTime) {
            cutscene.letterboxRetractTimer += realDeltaTime;
            const float t = std::clamp(cutscene.letterboxRetractTimer / kLetterboxRetractDuration, 0.0f, 1.0f);

            if(t >= 1.0f) {
                HideUiSprite(registry, cutscene.letterboxTopEntity);
                HideUiSprite(registry, cutscene.letterboxBottomEntity);
                cutscene.retractingLetterbox = false;
                return;
            }

            const float windowWidth  = ctx.window ? static_cast<float>(ctx.window->GetWidth()) : 1920.0f;
            const float windowHeight = ctx.window ? static_cast<float>(ctx.window->GetHeight()) : 1080.0f;
            const float barHeight    = windowHeight * kLetterboxHeightRatio * (1.0f - t);

            StretchSprite(registry, ctx, cutscene.letterboxTopEntity, windowWidth * 0.5f, barHeight * 0.5f, windowWidth, barHeight,
                          kLetterboxColor);
            StretchSprite(registry, ctx, cutscene.letterboxBottomEntity, windowWidth * 0.5f, windowHeight - barHeight * 0.5f, windowWidth,
                          barHeight, kLetterboxColor);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief カットシーン再生状態とレターボックス／スキップ案内UIを非表示で作る
    //-------------------------------------------------------------
    void CreateCutscenePlayback(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                Tsukino::ECS::Entity playerEntity) {
        CutsceneComponent& cutscene = registry.AddComponent<CutsceneComponent>(playerEntity);

        cutscene.letterboxTopEntity    = CreateUiRectEntity(registry, context, CombatAndroid::UI::kCutsceneLetterbox);
        cutscene.letterboxBottomEntity = CreateUiRectEntity(registry, context, CombatAndroid::UI::kCutsceneLetterbox);
        cutscene.skipHintTextEntity    = CreateUiTextEntity(registry, CombatAndroid::UI::kCutsceneSkipHint, UiTextAlign::Center);
    }

    //-------------------------------------------------------------
    //! @brief カットシーンを1本再生する
    //-------------------------------------------------------------
    void PlayCutscene(Tsukino::ECS::Registry& registry, std::vector<CutsceneShot> shots, bool liveWorld) {
        CutsceneComponent* cutscene = FindCutscene(registry);
        if(!cutscene) {
            Tsukino::Core::Log::Error("CutsceneSystem - CreateCutscenePlayback was not called; PlayCutscene ignored.");
            return;
        }

        if(shots.empty()) {
            Tsukino::Core::Log::Warn("CutsceneSystem - PlayCutscene called with an empty shot list; ignored.");
            return;
        }

        // 既に何か再生中、またはスキル選択・ポーズ・リザルトが既に進行を止めている場合は
        // 同時に2つ動かさない（IsGameplayFrozenはこの時点でIsCutsceneActiveも見るが、
        // playingはまだfalseなので正しく「他の要因」だけを見る）
        if(cutscene->playing || IsGameplayFrozen(registry)) {
            Tsukino::Core::Log::Warn("CutsceneSystem - PlayCutscene ignored because a cutscene or another freeze is already active.");
            return;
        }

        // 止めている間にヒットストップが残ると、再開した後もスローが続いてしまう
        ClearAllHitStop(registry);

        cutscene->shots            = std::move(shots);
        cutscene->playing          = true;
        cutscene->currentShotIndex = -1;
        cutscene->shotTimer        = 0.0f;
        cutscene->hasSpringState   = false;
        cutscene->liveWorld        = liveWorld;
    }

    //-------------------------------------------------------------
    //! @brief 今カットシーンを再生中かを問い合わせる
    //-------------------------------------------------------------
    bool IsCutsceneActive(Tsukino::ECS::Registry& registry) {
        const CutsceneComponent* cutscene = FindCutscene(registry);
        return cutscene && cutscene->playing;
    }

    //-------------------------------------------------------------
    //! @brief 世界の時間を止めないカットシーンを再生中かを問い合わせる
    //-------------------------------------------------------------
    bool IsLiveCutsceneActive(Tsukino::ECS::Registry& registry) {
        const CutsceneComponent* cutscene = FindCutscene(registry);
        return cutscene && cutscene->playing && cutscene->liveWorld;
    }

    //-------------------------------------------------------------
    //! @brief 再生中のショット番号を問い合わせる
    //-------------------------------------------------------------
    int GetCutsceneShotIndex(Tsukino::ECS::Registry& registry) {
        const CutsceneComponent* cutscene = FindCutscene(registry);
        if(!cutscene || !cutscene->playing)
            return -1;
        return std::max(cutscene->currentShotIndex, 0);
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void CutsceneSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        CutsceneComponent* cutscene = FindCutscene(registry);
        if(!cutscene)
            return;

        // 演出はWorldTimeContext::realDeltaTimeで進める（自分自身が引き起こすフリーズで
        // 止まらないようにするため。TpsCameraSystemの死亡演出と同じ理由）
        const float rawDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : 0.0f;
        const float realDeltaTime = std::min(rawDeltaTime, kMaxShotStep);

        if(!cutscene->playing) {
            // 再生は終わっていても、上下の帯だけ退場アニメーションを追いかけて進める
            if(cutscene->retractingLetterbox)
                UpdateLetterboxRetraction(registry, *ctx, *cutscene, realDeltaTime);
            return;
        }

        if(cutscene->shots.empty()) {
            StopCutscene(registry, *cutscene);
            return;
        }

        // PhysicsSystemはdeltaTime<=0でも1/60秒ぶん必ずステップするため、
        // 毎フレーム移動入力を潰しておく（PauseMenuSystem等と同じ作法）
        SuppressAllMoveInput(registry);

        //-------------------------------------------------------------
        // スキップ入力。ポーズ／スキル選択の決定と同じキーで打ち切れるようにする
        //-------------------------------------------------------------
        const bool windowFocused = ctx->window && ctx->window->IsFocused();
        if(windowFocused && ctx->inputSystem
           && (IsGameMenuConfirmPressed(*ctx->inputSystem) || IsGameMenuCancelPressed(*ctx->inputSystem))) {
            StopCutscene(registry, *cutscene);
            return;
        }

        if(cutscene->currentShotIndex < 0)
            cutscene->currentShotIndex = 0;

        const CutsceneShot& shot = cutscene->shots[static_cast<size_t>(cutscene->currentShotIndex)];

        //-------------------------------------------------------------
        // 基準位置の解決。targetEntityが有効ならその今フレームの位置を使う
        //-------------------------------------------------------------
        hlslpp::float3 basePosition = shot.targetPoint;
        if(shot.targetEntity != entt::null && registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(shot.targetEntity))
            basePosition = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(shot.targetEntity).position;

        //-------------------------------------------------------------
        // yaw=0, pitch=0のとき-Z方向（後方）を基準とした球面座標でオフセットを求める
        // （TpsCameraSystemと同じ式）。保持中はyawDriftSpeedぶんだけ回す
        //-------------------------------------------------------------
        const float yaw            = shot.yaw + shot.yawDriftSpeed * cutscene->shotTimer;
        const float horizontalDist = shot.distance * std::cos(shot.pitch);

        hlslpp::float3 offset;
        offset.x = horizontalDist * std::sin(yaw);
        offset.y = shot.height + shot.distance * std::sin(shot.pitch);
        offset.z = -horizontalDist * std::cos(yaw);

        const hlslpp::float3 desiredPosition = basePosition + offset;
        const hlslpp::float3 desiredLookAt   = basePosition + hlslpp::float3(0.0f, shot.lookHeight, 0.0f);
        const float          desiredFov      = shot.fov;

        if(!cutscene->hasSpringState) {
            // カットシーンへの入り、または前のショットからの継続点が無い最初のtickは
            // ばねで寄らせず、狙ったframingへ瞬時にスナップする（ハードカット）
            cutscene->springPosition       = desiredPosition;
            cutscene->springVelocity       = hlslpp::float3(0.0f, 0.0f, 0.0f);
            cutscene->lookAtSpringPosition = desiredLookAt;
            cutscene->lookAtSpringVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);
            cutscene->springFov            = desiredFov;
            cutscene->springFovVelocity    = 0.0f;
            cutscene->hasSpringState       = true;
        } else {
            Spring::Step(cutscene->springPosition, cutscene->springVelocity, desiredPosition, shot.blendFrequency, 1.0f, realDeltaTime);
            Spring::Step(cutscene->lookAtSpringPosition, cutscene->lookAtSpringVelocity, desiredLookAt, shot.blendFrequency, 1.0f,
                       realDeltaTime);
            Spring::Step(cutscene->springFov, cutscene->springFovVelocity, desiredFov, shot.blendFrequency, 1.0f, realDeltaTime);
        }

        //-------------------------------------------------------------
        // TPSカメラのTransform/CameraComponent::fovへ直接書く。
        // TpsCameraSystem（Camera3D）はIsCutsceneActiveを見て何もしないので、
        // この後上書きされることはない
        //-------------------------------------------------------------
        auto cameraView = registry.View<TpsCameraComponent, Tsukino::BuiltIn::ECS::TransformComponent, Tsukino::BuiltIn::ECS::CameraComponent>();
        for(entt::entity cameraEntity : cameraView) {
            auto& transform = cameraView.get<Tsukino::BuiltIn::ECS::TransformComponent>(cameraEntity);
            auto& camera    = cameraView.get<Tsukino::BuiltIn::ECS::CameraComponent>(cameraEntity);

            transform.position = cutscene->springPosition;
            transform.dirty    = true;

            camera.useLookAt    = true;
            camera.lookAtTarget = cutscene->lookAtSpringPosition;
            camera.fov          = cutscene->springFov;
            // farZはポップが見える値ではないので、ばねで寄せず毎フレーム直接書く
            camera.farZ         = shot.farZ;
            camera.dirty        = true;
            break;    // TPSカメラは1体だけの想定
        }

        //-------------------------------------------------------------
        // レターボックスとスキップ案内の表示
        //-------------------------------------------------------------
        const float windowWidth  = ctx->window ? static_cast<float>(ctx->window->GetWidth()) : 1920.0f;
        const float windowHeight = ctx->window ? static_cast<float>(ctx->window->GetHeight()) : 1080.0f;
        const float barHeight    = windowHeight * kLetterboxHeightRatio;

        StretchSprite(registry, *ctx, cutscene->letterboxTopEntity, windowWidth * 0.5f, barHeight * 0.5f, windowWidth, barHeight,
                      kLetterboxColor);
        StretchSprite(registry, *ctx, cutscene->letterboxBottomEntity, windowWidth * 0.5f, windowHeight - barHeight * 0.5f, windowWidth,
                      barHeight, kLetterboxColor);
        PlaceUiText(registry, cutscene->skipHintTextEntity, windowWidth * 0.5f, windowHeight - barHeight * 0.5f, GetUiTextScale(UiTextSize::Body),
                   ctx->inputSystem ? FormatInputText(*ctx->inputSystem, L"{Confirm} / {Cancel} でスキップ") : std::wstring(L"決定でスキップ"),
                   kSkipHintColor);
        UiPulse(registry, cutscene->skipHintTextEntity, 0.8f, true);    // 案内を止まったままにしない（ムービー中なので実時間）

        //-------------------------------------------------------------
        // ショットの進行。durationを超えたら次へ、最後まで終えたら停止する
        //-------------------------------------------------------------
        cutscene->shotTimer += realDeltaTime;
        if(cutscene->shotTimer < shot.duration)
            return;

        cutscene->shotTimer = 0.0f;
        ++cutscene->currentShotIndex;
        if(cutscene->currentShotIndex >= static_cast<int>(cutscene->shots.size()))
            StopCutscene(registry, *cutscene);
    }
}    // namespace CombatAndroid::ECS
