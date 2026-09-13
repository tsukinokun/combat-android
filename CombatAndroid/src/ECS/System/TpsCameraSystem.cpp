//-------------------------------------------------------------
//! @file   TpsCameraSystem.cpp
//! @brief  TpsCameraSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/TpsCameraSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Component/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>

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
        void StepSpring(hlslpp::float3& position, hlslpp::float3& velocity, const hlslpp::float3& target, float frequency, float damping,
                        float deltaTime) {
            if(deltaTime <= 0.0f)
                return;

            const float omega = kTwoPi * std::max(frequency, 1.0e-3f);    // Hz → 角周波数(rad/s)
            const float zeta  = std::max(damping, 0.0f);

            // 目標からのずれとして解く
            const hlslpp::float3 x0 = position - target;
            const hlslpp::float3 v0 = velocity;

            hlslpp::float3 x;
            hlslpp::float3 v;

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
                const float          envelope = std::exp(-omega * deltaTime);
                const hlslpp::float3 k        = v0 + x0 * omega;

                x = (x0 + k * deltaTime) * envelope;
                v = (v0 - k * (omega * deltaTime)) * envelope;
            } else {
                // 過減衰（行き過ぎずにゆっくり収まる）
                const float          root = std::sqrt(zeta * zeta - 1.0f);
                const float          r1   = -omega * (zeta - root);
                const float          r2   = -omega * (zeta + root);
                const hlslpp::float3 c1   = (v0 - x0 * r2) / (r1 - r2);
                const hlslpp::float3 c2   = x0 - c1;
                const float          e1   = std::exp(r1 * deltaTime);
                const float          e2   = std::exp(r2 * deltaTime);

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
        m_damagedConnection = eventBus.Subscribe<PlayerDamagedEvent>([this](const PlayerDamagedEvent& event) { OnPlayerDamaged(event); });
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
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TpsCameraSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        //-------------------------------------------------------------
        // スキル選択メニュー中はカメラの旋回を止める。
        // 下のyaw/pitchの加算はdeltaTimeを掛けていないため、シーンがdeltaTime=0を
        // 渡してきても回り続けてしまう（追従の補間だけが止まり、メニューを閉じた瞬間に
        // 溜まった角度へ一気に振れる）。
        // 併せてwasCapturedLastFrameを倒しておくと、復帰後の最初の1フレームぶんの
        // マウス移動量は上の「キャプチャ復帰フレームは旋回に使わない」分岐が捨ててくれる
        //-------------------------------------------------------------
        if(IsSkillSelectActive(registry)) {
            auto pausedView = registry.View<TpsCameraComponent>();
            pausedView.each([](TpsCameraComponent& tpsCamera) { tpsCamera.wasCapturedLastFrame = false; });
            m_pendingShakeDamage = 0.0f;
            return;
        }

        Tsukino::Input::InputSystem* inputSystem = ctx->inputSystem;

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

            //-------------------------------------------------------------
            // Escキーでマウスキャプチャ（カーソル非表示＋旋回操作）のON/OFFを切り替える
            //-------------------------------------------------------------
            if(windowFocused && inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::Escape)) {
                tpsCamera.mouseCaptured = !tpsCamera.mouseCaptured;
            }

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
                tpsCamera.yaw -= static_cast<float>(mouseDx) * tpsCamera.mouseSensitivity;
                tpsCamera.pitch += static_cast<float>(mouseDy) * tpsCamera.mouseSensitivity;
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

            //-------------------------------------------------------------
            // ばねで追従する。初回と、リトライ等で目標が大きく飛んだときは
            // ばねで引っ張ると画面を横切って飛んでくるので、目標へ置き直す
            //-------------------------------------------------------------
            float followGap = hlslpp::length(desiredPosition - transform.position);
            if(!tpsCamera.hasFollowSpringState || followGap > tpsCamera.followSpringResetDistance) {
                transform.position             = desiredPosition;
                tpsCamera.followSpringVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);
                tpsCamera.hasFollowSpringState = true;
            } else {
                hlslpp::float3 position = transform.position;
                StepSpring(position, tpsCamera.followSpringVelocity, desiredPosition, tpsCamera.followSpringFrequency,
                           tpsCamera.followSpringDamping, deltaTime);
                transform.position = position;
            }
            transform.dirty = true;

            hlslpp::float3 lookAtTarget = targetTransform.position + hlslpp::float3(0.0f, tpsCamera.lookHeight, 0.0f);

            //-------------------------------------------------------------
            // 被弾時の揺れ。
            // 画面の左右・上下の平面内でランダムな向きに初速を与え、ばねで0へ引き戻す。
            // 奥行き方向へ揺らすと注視点が前後するだけで画面はほとんど動かないので、
            // 画面に平行な向きに限る
            //-------------------------------------------------------------
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

        m_pendingShakeDamage = 0.0f;
    }
}    // namespace CombatAndroid::ECS
