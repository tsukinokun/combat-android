//-------------------------------------------------------------
//! @file   TpsCameraSystem.cpp
//! @brief  TpsCameraSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/TpsCameraSystem.hpp>
#include <CombatAndroid/ECS/Utility/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Component/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Utility/WorldTimeContext.hpp>
#include <CombatAndroid/ECS/Utility/GameSettings.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Window.hpp>

#include <hlsl++.h>
#include <cmath>
#include <algorithm>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kTwoPi = 6.28318531f;

        //-------------------------------------------------------------
        //! @brief 減衰調和振動子（ばね・ダンパー）を1ステップ進める
        //! @tparam T        float または hlslpp::float3
        //! @param position  [in,out] 現在位置
        //! @param velocity  [in,out] 現在速度
        //! @param target    [in]     引き寄せられる先（このステップの間は動かないものとして扱う）
        //! @param frequency [in]     固有振動数（Hz）
        //! @param damping   [in]     減衰比（1.0で行き過ぎ無し）
        //! @param deltaTime [in]     経過秒
        //! @note  運動方程式の解析解でdeltaTimeぶん厳密に進める。
        //!        浮遊武器の追従（CombatSystem.cpp の followSpring）は陰的オイラーだが、
        //!        あちらは1.6Hzとゆっくりなので問題にならない。被弾の揺れのように8Hzで振らせると、
        //!        陰的オイラーは計算上の減衰で振れ幅が半分ほどに潰れ、しかもフレームレートで
        //!        振れ幅が変わってしまう（30fpsと144fpsで1.6倍違った）。解析解ならどのフレームレートでも
        //!        同じ揺れになり、deltaTimeが大きくても発散しない
        //-------------------------------------------------------------
        template <typename T>
        void StepSpring(T& position, T& velocity, const T& target, float frequency, float damping, float deltaTime) {
            if(deltaTime <= 0.0f)
                return;

            const float omega = kTwoPi * std::max(frequency, 1.0e-3f);    // Hz → 角周波数(rad/s)
            const float zeta  = std::max(damping, 0.0f);

            // 目標からのずれとして解く
            const T x0 = position - target;
            const T v0 = velocity;

            T x;
            T v;

            if(zeta < 0.9999f) {
                // 減衰振動（行き過ぎて何度か揺り戻す）
                const float decay     = zeta * omega;
                const float omegaD    = omega * std::sqrt(1.0f - zeta * zeta);
                const float envelope  = std::exp(-decay * deltaTime);
                const float cosTerm   = std::cos(omegaD * deltaTime);
                const float sinTerm   = std::sin(omegaD * deltaTime);

                x = (x0 * cosTerm + (v0 + x0 * decay) * (sinTerm / omegaD)) * envelope;
                v = (v0 * cosTerm - (x0 * (omega * omega) + v0 * decay) * (sinTerm / omegaD)) * envelope;
            } else if(zeta <= 1.0001f) {
                // 臨界減衰（行き過ぎずに最短で収まる）
                const float envelope = std::exp(-omega * deltaTime);
                const T     k        = v0 + x0 * omega;

                x = (x0 + k * deltaTime) * envelope;
                v = (v0 - k * (omega * deltaTime)) * envelope;
            } else {
                // 過減衰（行き過ぎずにゆっくり収まる）
                const float root = std::sqrt(zeta * zeta - 1.0f);
                const float r1   = -omega * (zeta - root);
                const float r2   = -omega * (zeta + root);
                const T     c1   = (v0 - x0 * r2) / (r1 - r2);
                const T     c2   = x0 - c1;
                const float e1   = std::exp(r1 * deltaTime);
                const float e2   = std::exp(r2 * deltaTime);

                x = c1 * e1 + c2 * e2;
                v = c1 * (r1 * e1) + c2 * (r2 * e2);
            }

            position = target + x;
            velocity = v;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief PlayerDamagedEventの購読を開始する
    //-------------------------------------------------------------
    void TpsCameraSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_damagedConnection  = eventBus.Subscribe<PlayerDamagedEvent>([this](const PlayerDamagedEvent& event) { OnPlayerDamaged(event); });
        m_finisherConnection = eventBus.Subscribe<PlayerFinisherEvent>([this](const PlayerFinisherEvent& event) { OnPlayerFinisher(event); });
    }

    //-------------------------------------------------------------
    //! @brief 被弾通知のハンドラ
    //-------------------------------------------------------------
    void TpsCameraSystem::OnPlayerDamaged(const PlayerDamagedEvent& event) {
        // プレイヤーは1体だけの想定なので対象は選ばない。
        // CombatSystem（WeaponAttach）がPublishし、このシステム（Camera3D）はその後に動くため、
        // 被弾したフレームのうちに揺れ始める
        m_pendingShakeDamage += std::max(event.damage, 0.0f);
    }

    //-------------------------------------------------------------
    //! @brief 大技通知のハンドラ
    //-------------------------------------------------------------
    void TpsCameraSystem::OnPlayerFinisher(const PlayerFinisherEvent& event) {
        // 寄り始めるのはインパクトの瞬間（Updateで数える）
        const float delay        = std::max(event.impactDelay, 0.0f);
        m_pendingZoomImpactDelay = (m_pendingZoomImpactDelay < 0.0f) ? delay : std::min(m_pendingZoomImpactDelay, delay);
    }
    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TpsCameraSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        //-------------------------------------------------------------
        // メニュー（スキル選択・ポーズ・リザルト）中はカメラの旋回を止め、カーソルを解放する。
        // 下のyaw/pitchの加算はdeltaTimeを掛けていないため、シーンがdeltaTime=0を
        // 渡してきても回り続けてしまう（追従の補間だけが止まり、メニューを閉じた瞬間に
        // 溜まった角度へ一気に振れる）。
        // 併せてwasCapturedLastFrameを倒しておくと、復帰後の最初の1フレームぶんの
        // マウス移動量は上の「キャプチャ復帰フレームは旋回に使わない」分岐が捨ててくれる
        //-------------------------------------------------------------
        if(IsGameplayFrozen(registry)) {
            // ポーズ中にウィンドウを動かしたり他のアプリへ移ったりできるよう、カーソルを出す
            if(ctx->window)
                ctx->window->SetCursorVisible(true);

            auto pausedView = registry.View<TpsCameraComponent>();
            pausedView.each([](TpsCameraComponent& tpsCamera) { tpsCamera.wasCapturedLastFrame = false; });
            m_pendingShakeDamage     = 0.0f;
            m_pendingZoomImpactDelay = -1.0f;
            return;
        }

        Tsukino::Input::InputSystem* inputSystem = ctx->inputSystem;

        //-------------------------------------------------------------
        // 大技のスローが掛かっていない実時間。カメラの寄りはこちらで動かす
        // （スローと同じ瞬間に寄り始めても、寄る速さまで遅くなると「素早く寄る」感じが消えるため）
        //-------------------------------------------------------------
        const float realDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;

        //-------------------------------------------------------------
        // マウスの移動量を取得（このフレームの旋回入力）
        //-------------------------------------------------------------
        Tsukino::i32 rawMouseDx = 0, rawMouseDy = 0;
        inputSystem->GetMouseDelta(&rawMouseDx, &rawMouseDy);

        //-------------------------------------------------------------
        // ウィンドウがフォーカスされているか（Alt+Tab等で裏にいる間はカーソルを奪わない）
        //-------------------------------------------------------------
        bool windowFocused = ctx->window && ctx->window->IsFocused();

        auto view = registry.View<TpsCameraComponent, Tsukino::BuiltIn::ECS::TransformComponent, Tsukino::BuiltIn::ECS::CameraComponent>();
        view.each([&](entt::entity                                  entity,
                     TpsCameraComponent&                            tpsCamera,
                     Tsukino::BuiltIn::ECS::TransformComponent&    transform,
                     Tsukino::BuiltIn::ECS::CameraComponent&       camera) {
            //-------------------------------------------------------------
            // 追従対象が未設定なら、プレイヤーエンティティを探して設定する
            //-------------------------------------------------------------
            if(tpsCamera.target == entt::null) {
                auto playerView = registry.View<PlayerComponent>();
                if(!playerView.empty())
                    tpsCamera.target = playerView.front();
            }

            if(tpsCamera.target == entt::null || !registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(tpsCamera.target))
                return;

            Tsukino::BuiltIn::ECS::TransformComponent& targetTransform =
                registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(tpsCamera.target);

            // Escはポーズメニュー（PauseMenuSystem）が使う。キャプチャを外したいときはポーズを開く
            bool shouldCapture = tpsCamera.mouseCaptured && windowFocused;

            if(ctx->window)
                ctx->window->SetCursorVisible(!shouldCapture);

            Tsukino::i32 mouseDx = rawMouseDx;
            Tsukino::i32 mouseDy = rawMouseDy;

            if(shouldCapture) {
                // 直前フレームまでキャプチャが外れていた場合、カーソルがどこにあったか分からないため
                // このフレームの分は旋回に使わず、中央への位置合わせだけ行う
                if(!tpsCamera.wasCapturedLastFrame) {
                    mouseDx = 0;
                    mouseDy = 0;
                }

                //-------------------------------------------------------------
                // マウス移動量でyaw/pitchを更新する
                // 左右は反転させる（マウスを右へ動かすとカメラは左へ旋回する）
                //-------------------------------------------------------------
                // 感度はコンポーネントの基準値にオプションの倍率を掛ける
                const float sensitivity = tpsCamera.mouseSensitivity * GetGameSettings().mouseSensitivityScale;
                tpsCamera.yaw -= static_cast<float>(mouseDx) * sensitivity;
                tpsCamera.pitch += static_cast<float>(mouseDy) * sensitivity;
                tpsCamera.pitch = std::clamp(tpsCamera.pitch, tpsCamera.minPitch, tpsCamera.maxPitch);

                //-------------------------------------------------------------
                // カーソルを中央へ戻し、InputSystem側の座標も同期させる
                // （同期しないと、戻した分が次フレームの移動量として誤検出される）
                //-------------------------------------------------------------
                if(ctx->window) {
                    ctx->window->CenterCursor();

                    Tsukino::i32 centerX = ctx->window->GetWidth() / 2;
                    Tsukino::i32 centerY = ctx->window->GetHeight() / 2;
                    inputSystem->SetMousePosition(centerX, centerY);
                }
            }

            tpsCamera.wasCapturedLastFrame = shouldCapture;

            //-------------------------------------------------------------
            // yaw=0, pitch=0のとき-Z方向（後方）を基準とした球面座標でオフセットを求める
            //-------------------------------------------------------------
            float horizontalDist = tpsCamera.distance * std::cos(tpsCamera.pitch);

            hlslpp::float3 offset;
            offset.x = horizontalDist * std::sin(tpsCamera.yaw);
            offset.y = tpsCamera.height + tpsCamera.distance * std::sin(tpsCamera.pitch);
            offset.z = -horizontalDist * std::cos(tpsCamera.yaw);

            hlslpp::float3 desiredPosition = targetTransform.position + offset;

            // 見上げたときに球面上の位置が地面の下へ回り込まないよう、高さに下限を設ける
            const float minCameraHeight = tpsCamera.groundHeight + tpsCamera.minHeightAboveGround;
            desiredPosition.y           = std::max(static_cast<float>(desiredPosition.y), minCameraHeight);

            //-------------------------------------------------------------
            // ばねで追従する。初回と、リトライ等で目標が大きく飛んだときは
            // ばねで引っ張ると画面を横切って飛んでくるので、目標へ置き直す。
            // ばねの状態は大技のズームで寄るぶんを足す前の位置として別に持つ
            //-------------------------------------------------------------
            float followGap = hlslpp::length(desiredPosition - tpsCamera.followSpringPosition);
            if(!tpsCamera.hasFollowSpringState || followGap > tpsCamera.followSpringResetDistance) {
                tpsCamera.followSpringPosition = desiredPosition;
                tpsCamera.followSpringVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);
                tpsCamera.hasFollowSpringState = true;
            } else {
                StepSpring(tpsCamera.followSpringPosition, tpsCamera.followSpringVelocity, desiredPosition, tpsCamera.followSpringFrequency,
                           tpsCamera.followSpringDamping, deltaTime);

                // 目標を下限で止めても、ばねは行き過ぎるので地面の下まで沈みうる。
                // 床に当たったら下向きの速度を捨てて、跳ね返らずにそこへ留める
                if(tpsCamera.followSpringPosition.y < minCameraHeight) {
                    tpsCamera.followSpringPosition.y = minCameraHeight;
                    tpsCamera.followSpringVelocity.y = std::max(static_cast<float>(tpsCamera.followSpringVelocity.y), 0.0f);
                }
            }

            hlslpp::float3 lookAtTarget = targetTransform.position + hlslpp::float3(0.0f, tpsCamera.lookHeight, 0.0f);

            //-------------------------------------------------------------
            // 大技のズーム。
            // インパクトの瞬間から zoomHoldAfterImpact のあいだ寄りの目標を1にし、切れたら0へ戻す。
            //
            // 世界のスロー（SlowMotionController）と同じ瞬間に寄り始めるよう、インパクトまでの待ちは
            // スローと同じ数え方にする：どちらもゲーム内時間で数え、イベントを受けたフレームは数えず
            // 次のフレームから減らす（スローはシーンが次のフレームの頭で進めるため）。
            // そのため、既に待っているぶんを先に減らしてから、今フレーム届いたぶんを合流させる
            //-------------------------------------------------------------
            if(tpsCamera.zoomStartTimer >= 0.0f) {
                tpsCamera.zoomStartTimer -= deltaTime;
                if(tpsCamera.zoomStartTimer <= 0.0f) {
                    tpsCamera.zoomStartTimer = -1.0f;
                    tpsCamera.zoomHoldTimer  = std::max(tpsCamera.zoomHoldTimer, tpsCamera.zoomHoldAfterImpact);
                }
            }

            if(m_pendingZoomImpactDelay >= 0.0f) {
                tpsCamera.zoomStartTimer = (tpsCamera.zoomStartTimer < 0.0f) ? m_pendingZoomImpactDelay
                                                                             : std::min(tpsCamera.zoomStartTimer, m_pendingZoomImpactDelay);
            }

            //-------------------------------------------------------------
            // 寄りは素早く、戻りはゆっくりにしたいので、どちらへ向かうかでばねの速さを変える。
            // 減衰比は1.0（行き過ぎ無し）にして、寄りきった所で画面が揺り戻さないようにする。
            // 保持とばねは実時間で進める（インパクトまでの待ちだけはスローと揃えるためゲーム内時間）
            //-------------------------------------------------------------
            const float zoomTarget = (tpsCamera.zoomHoldTimer > 0.0f) ? 1.0f : 0.0f;
            tpsCamera.zoomHoldTimer = std::max(tpsCamera.zoomHoldTimer - realDeltaTime, 0.0f);

            const float zoomFrequency = (zoomTarget > 0.0f) ? tpsCamera.zoomInFrequency : tpsCamera.zoomOutFrequency;
            StepSpring(tpsCamera.zoomAmount, tpsCamera.zoomVelocity, zoomTarget, zoomFrequency, 1.0f, realDeltaTime);

            // 注視点へ向かって、距離を縮めるぶんだけ前進する
            hlslpp::float3 cameraPosition = tpsCamera.followSpringPosition;
            hlslpp::float3 toLookAt       = lookAtTarget - cameraPosition;
            float          toLookAtLength = hlslpp::length(toLookAt);
            if(toLookAtLength > 1.0e-3f) {
                const float approach = tpsCamera.distance * (1.0f - tpsCamera.zoomDistanceScale) * tpsCamera.zoomAmount;
                cameraPosition       = cameraPosition + (toLookAt / toLookAtLength) * std::min(approach, toLookAtLength * 0.9f);
            }

            // 寄りは注視点（頭上）へ向かうので下がることは無いが、念のため最終位置にも下限を効かせる
            cameraPosition.y = std::max(static_cast<float>(cameraPosition.y), minCameraHeight);

            transform.position = cameraPosition;
            transform.dirty    = true;

            // 画角はシーンが設定した値を基準にする（最初のフレームで覚える）
            if(!tpsCamera.hasBaseFov) {
                tpsCamera.baseFov    = camera.fov;
                tpsCamera.hasBaseFov = true;
            }
            camera.fov = tpsCamera.baseFov * (1.0f + (tpsCamera.zoomFovScale - 1.0f) * tpsCamera.zoomAmount);

            //-------------------------------------------------------------
            // 被弾時の揺れ。
            // 画面の左右・上下の平面内でランダムな向きに初速を与え、ばねで0へ引き戻す。
            // 奥行き方向へ揺らすと注視点が前後するだけで画面はほとんど動かないので、
            // 画面に平行な向きに限る
            //-------------------------------------------------------------
            // オプションで画面揺れを切っているときは、溜まった揺れを捨てて何もしない
            if(!GetGameSettings().screenShakeEnabled)
                m_pendingShakeDamage = 0.0f;

            if(m_pendingShakeDamage > 0.0f) {
                const float referenceDamage = std::max(tpsCamera.shakeReferenceDamage, 1.0f);
                const float scale = std::clamp(m_pendingShakeDamage / referenceDamage, tpsCamera.shakeMinScale, tpsCamera.shakeMaxScale);

                hlslpp::float3 forward       = lookAtTarget - transform.position;
                float          forwardLength = hlslpp::length(forward);
                if(forwardLength > 1.0e-3f) {
                    forward = hlslpp::normalize(forward);

                    hlslpp::float3 right       = hlslpp::cross(hlslpp::float3(0.0f, 1.0f, 0.0f), forward);
                    float          rightLength = hlslpp::length(right);
                    // 真上・真下を向いているときは左右が決まらないので、水平の固定軸で代用する
                    right = (rightLength > 1.0e-3f) ? hlslpp::normalize(right) : hlslpp::float3(1.0f, 0.0f, 0.0f);

                    hlslpp::float3 up = hlslpp::cross(forward, right);

                    std::uniform_real_distribution<float> angleDist(0.0f, kTwoPi);
                    const float                           angle = angleDist(m_rng);

                    hlslpp::float3 direction = right * std::cos(angle) + up * std::sin(angle);
                    tpsCamera.shakeVelocity  = tpsCamera.shakeVelocity + direction * (tpsCamera.shakeImpulse * scale);
                }
            }

            StepSpring(tpsCamera.shakeOffset, tpsCamera.shakeVelocity, hlslpp::float3(0.0f, 0.0f, 0.0f), tpsCamera.shakeFrequency,
                       tpsCamera.shakeDamping, deltaTime);

            //-------------------------------------------------------------
            // プレイヤーの頭のあたりを注視する（揺れのぶんだけずらす）
            //-------------------------------------------------------------
            camera.useLookAt    = true;
            camera.lookAtTarget = lookAtTarget + tpsCamera.shakeOffset;
            camera.dirty        = true;
        });

        m_pendingShakeDamage     = 0.0f;
        m_pendingZoomImpactDelay = -1.0f;
    }
}    // namespace CombatAndroid::ECS
