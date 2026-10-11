//-------------------------------------------------------------
//! @file   TpsCameraSystem.cpp
//! @brief  TpsCameraSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/World/TpsCameraSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/RunResultSystem.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Component/World/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>
#include <CombatAndroid/ECS/Utility/Save/GameSettings.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Core/Math/Spring.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Window.hpp>

#include <hlsl++.h>
#include <cmath>
#include <algorithm>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kTwoPi = 6.28318531f;
        namespace Spring = Tsukino::Core::Math::Spring;    // 減衰ばね（エンジンの Spring.hpp）
    }    // namespace

    //-------------------------------------------------------------
    //! @brief PlayerDamagedEventの購読を開始する
    //-------------------------------------------------------------
    void TpsCameraSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_damagedConnection  = eventBus.Subscribe<PlayerDamagedEvent>([this](const PlayerDamagedEvent& event) { OnPlayerDamaged(event); });
        m_finisherConnection = eventBus.Subscribe<PlayerFinisherEvent>([this](const PlayerFinisherEvent& event) { OnPlayerFinisher(event); });
        m_heavyStrikeConnection =
            eventBus.Subscribe<PlayerHeavyStrikeEvent>([this](const PlayerHeavyStrikeEvent& event) { OnPlayerHeavyStrike(event); });
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
    //! @brief 連撃の最終段の通知ハンドラ
    //-------------------------------------------------------------
    void TpsCameraSystem::OnPlayerHeavyStrike(const PlayerHeavyStrikeEvent& event) {
        // 揺らすのはインパクトの瞬間（Updateで数える）
        const float delay         = std::max(event.impactDelay, 0.0f);
        m_pendingStrikeShakeDelay = (m_pendingStrikeShakeDelay < 0.0f) ? delay : std::min(m_pendingStrikeShakeDelay, delay);
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
        //-------------------------------------------------------------
        // カットシーン中はCutsceneSystemがTransform/CameraComponent::fovを直接書くため、
        // 通常の追従は完全に退避する。メニュー中と違い操作を渡さないので、カーソルは出さない
        //-------------------------------------------------------------
        if(IsCutsceneActive(registry)) {
            auto pausedView = registry.View<TpsCameraComponent>();
            pausedView.each([](TpsCameraComponent& tpsCamera) { tpsCamera.wasCapturedLastFrame = false; });
            m_pendingShakeDamage     = 0.0f;
            m_pendingZoomImpactDelay = -1.0f;
            m_pendingStrikeShakeDelay = -1.0f;
            return;
        }

        // 走行が終わった後（リザルト）は世界が止まっても、倒れたプレイヤーの周りを回るカットを続ける。
        // マウスは使わせず、カーソルはメニュー用に出す
        const bool frozen = IsGameplayFrozen(registry);
        if(frozen && !IsRunEnded(registry)) {
            // ポーズ中にウィンドウを動かしたり他のアプリへ移ったりできるよう、カーソルを出す
            if(ctx->window)
                ctx->window->SetCursorVisible(true);

            auto pausedView = registry.View<TpsCameraComponent>();
            pausedView.each([](TpsCameraComponent& tpsCamera) { tpsCamera.wasCapturedLastFrame = false; });
            m_pendingShakeDamage     = 0.0f;
            m_pendingZoomImpactDelay = -1.0f;
            m_pendingStrikeShakeDelay = -1.0f;
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
            bool shouldCapture = tpsCamera.mouseCaptured && windowFocused && !frozen;

            if(ctx->window)
                ctx->window->SetCursorVisible(!shouldCapture);

            Tsukino::i32 mouseDx = rawMouseDx;
            Tsukino::i32 mouseDy = rawMouseDy;

            //-------------------------------------------------------------
            // 対象が倒れたか。倒れた後はマウスで回させず、カメラが自分で寄って回る
            //-------------------------------------------------------------
            const bool targetDead =
                registry.HasComponent<HealthComponent>(tpsCamera.target) && registry.GetComponent<HealthComponent>(tpsCamera.target).isDead;

            if(shouldCapture) {
                // 直前フレームまでキャプチャが外れていた場合、カーソルがどこにあったか分からないため
                // このフレームの分は旋回に使わず、中央への位置合わせだけ行う
                if(!tpsCamera.wasCapturedLastFrame || targetDead) {
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
            // 死亡時の寄り。倒れたら寄りの目標を1にし、ゆっくり寄せながら周りを回る。
            // リトライでは新しいシーンのカメラになるので、0へ戻す処理は要らない
            //-------------------------------------------------------------
            Spring::Step(tpsCamera.deathFocusAmount, tpsCamera.deathFocusVelocity, targetDead ? 1.0f : 0.0f, tpsCamera.deathFocusFrequency, 1.0f,
                       realDeltaTime);
            const float deathFocus = std::clamp(tpsCamera.deathFocusAmount, 0.0f, 1.0f);

            if(targetDead)
                tpsCamera.yaw += tpsCamera.deathOrbitSpeed * realDeltaTime;

            // 距離・回転中心の高さ・注視の高さ・見下ろし角を、寄りの量に応じて通常の値から寄せる
            const float distance   = std::lerp(tpsCamera.distance, tpsCamera.distance * tpsCamera.deathDistanceScale, deathFocus);
            const float height     = std::lerp(tpsCamera.height, tpsCamera.deathLookHeight, deathFocus);
            const float lookHeight = std::lerp(tpsCamera.lookHeight, tpsCamera.deathLookHeight, deathFocus);
            const float pitch      = std::lerp(tpsCamera.pitch, tpsCamera.deathPitch, deathFocus);

            //-------------------------------------------------------------
            // yaw=0, pitch=0のとき-Z方向（後方）を基準とした球面座標でオフセットを求める
            //-------------------------------------------------------------
            float horizontalDist = distance * std::cos(pitch);

            hlslpp::float3 offset;
            offset.x = horizontalDist * std::sin(tpsCamera.yaw);
            offset.y = height + distance * std::sin(pitch);
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
                // 倒れた後はリザルトで世界が止まってもカメラだけは回り続けるよう、実時間で追う
                Spring::Step(tpsCamera.followSpringPosition, tpsCamera.followSpringVelocity, desiredPosition, tpsCamera.followSpringFrequency,
                           tpsCamera.followSpringDamping, targetDead ? realDeltaTime : deltaTime);

                // 目標を下限で止めても、ばねは行き過ぎるので地面の下まで沈みうる。
                // 床に当たったら下向きの速度を捨てて、跳ね返らずにそこへ留める
                if(tpsCamera.followSpringPosition.y < minCameraHeight) {
                    tpsCamera.followSpringPosition.y = minCameraHeight;
                    tpsCamera.followSpringVelocity.y = std::max(static_cast<float>(tpsCamera.followSpringVelocity.y), 0.0f);
                }
            }

            hlslpp::float3 lookAtTarget = targetTransform.position + hlslpp::float3(0.0f, lookHeight, 0.0f);

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
            Spring::Step(tpsCamera.zoomAmount, tpsCamera.zoomVelocity, zoomTarget, zoomFrequency, 1.0f, realDeltaTime);

            // 寄るほど注視点を腰のあたりへ下げる。頭の上を見たまま寄ると足元と叩きつけた地面が切れるため
            lookAtTarget.y -= (lookHeight - tpsCamera.zoomLookHeight) * tpsCamera.zoomAmount;

            // 注視点へ向かって、距離を縮めるぶんだけ前進する
            hlslpp::float3 cameraPosition = tpsCamera.followSpringPosition;
            hlslpp::float3 toLookAt       = lookAtTarget - cameraPosition;
            float          toLookAtLength = hlslpp::length(toLookAt);
            if(toLookAtLength > 1.0e-3f) {
                const float approach = distance * (1.0f - tpsCamera.zoomDistanceScale) * tpsCamera.zoomAmount;
                cameraPosition       = cameraPosition + (toLookAt / toLookAtLength) * std::min(approach, toLookAtLength * 0.9f);
            }

            // 寄りは下げた注視点（腰）へ向かうので少し下がる。地面へ潜らないよう最終位置にも下限を効かせる
            cameraPosition.y = std::max(static_cast<float>(cameraPosition.y), minCameraHeight);

            transform.position = cameraPosition;
            transform.dirty    = true;

            // 画角はシーンが設定した値を基準にする（最初のフレームで覚える）
            if(!tpsCamera.hasBaseFov) {
                tpsCamera.baseFov    = camera.fov;
                tpsCamera.hasBaseFov = true;
            }
            camera.fov = tpsCamera.baseFov * (1.0f + (tpsCamera.zoomFovScale - 1.0f) * tpsCamera.zoomAmount)
                         * std::lerp(1.0f, tpsCamera.deathFovScale, deathFocus);

            //-------------------------------------------------------------
            // 被弾時と連撃最終段の揺れ。
            // 画面の左右・上下の平面内で初速を与え、ばねで0へ引き戻す。
            // 奥行き方向へ揺らすと注視点が前後するだけで画面はほとんど動かないので、
            // 画面に平行な向きに限る
            //-------------------------------------------------------------
            // オプションで画面揺れを切っているときは、溜まった揺れを捨てて何もしない
            // 倒れた後も敵の攻撃は当たり続けるが、寄りのカットを揺らさないよう捨てる
            const bool shakeAllowed = GetGameSettings().screenShakeEnabled && !targetDead;
            if(!shakeAllowed) {
                m_pendingShakeDamage       = 0.0f;
                m_pendingStrikeShakeDelay  = -1.0f;
                tpsCamera.strikeShakeTimer = -1.0f;
            }

            // 連撃最終段のインパクト待ち。大技のズームと同じく、既に待っているぶんを先に減らしてから
            // 今フレーム届いたぶんを合流させる（ズームの寄り始めと同じフレームに揺れ始める）
            bool strikeImpact = false;
            if(tpsCamera.strikeShakeTimer >= 0.0f) {
                tpsCamera.strikeShakeTimer -= deltaTime;
                if(tpsCamera.strikeShakeTimer <= 0.0f) {
                    tpsCamera.strikeShakeTimer = -1.0f;
                    strikeImpact               = true;
                }
            }

            if(m_pendingStrikeShakeDelay >= 0.0f) {
                tpsCamera.strikeShakeTimer = (tpsCamera.strikeShakeTimer < 0.0f) ? m_pendingStrikeShakeDelay
                                                                                 : std::min(tpsCamera.strikeShakeTimer, m_pendingStrikeShakeDelay);
            }

            if(m_pendingShakeDamage > 0.0f || strikeImpact) {
                hlslpp::float3 forward       = lookAtTarget - transform.position;
                float          forwardLength = hlslpp::length(forward);
                if(forwardLength > 1.0e-3f) {
                    forward = hlslpp::normalize(forward);

                    hlslpp::float3 right       = hlslpp::cross(hlslpp::float3(0.0f, 1.0f, 0.0f), forward);
                    float          rightLength = hlslpp::length(right);
                    // 真上・真下を向いているときは左右が決まらないので、水平の固定軸で代用する
                    right = (rightLength > 1.0e-3f) ? hlslpp::normalize(right) : hlslpp::float3(1.0f, 0.0f, 0.0f);

                    hlslpp::float3 up = hlslpp::cross(forward, right);

                    // 被弾はランダムな向きへ、ダメージに応じた強さで
                    if(m_pendingShakeDamage > 0.0f) {
                        const float referenceDamage = std::max(tpsCamera.shakeReferenceDamage, 1.0f);
                        const float scale = std::clamp(m_pendingShakeDamage / referenceDamage, tpsCamera.shakeMinScale, tpsCamera.shakeMaxScale);

                        std::uniform_real_distribution<float> angleDist(0.0f, kTwoPi);
                        const float                           angle = angleDist(m_rng);

                        hlslpp::float3 direction = right * std::cos(angle) + up * std::sin(angle);
                        tpsCamera.shakeVelocity  = tpsCamera.shakeVelocity + direction * (tpsCamera.shakeImpulse * scale);
                    }

                    // 最終段の叩きつけは、注視点を下へ叩いて画面をガクッと沈ませる
                    if(strikeImpact)
                        tpsCamera.shakeVelocity = tpsCamera.shakeVelocity - up * tpsCamera.strikeShakeImpulse;
                }
            }

            Spring::Step(tpsCamera.shakeOffset, tpsCamera.shakeVelocity, hlslpp::float3(0.0f, 0.0f, 0.0f), tpsCamera.shakeFrequency,
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
        m_pendingStrikeShakeDelay = -1.0f;
    }
}    // namespace CombatAndroid::ECS
