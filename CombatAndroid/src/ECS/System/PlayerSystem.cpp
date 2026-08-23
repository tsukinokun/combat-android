//-------------------------------------------------------------
//! @file   PlayerSystem.cpp
//! @brief  PlayerSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PlayerSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/TpsCameraComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

#include <hlsl++.h>
#include <cmath>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PlayerSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        //-------------------------------------------------------------
        // コンテキストの取得
        //-------------------------------------------------------------
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        Tsukino::Input::InputSystem* inputSystem = ctx->inputSystem;

        //-------------------------------------------------------------
        // TPSカメラのyawを取得する（カメラ基準の移動方向を求めるため）
        //-------------------------------------------------------------
        float cameraYaw = 0.0f;
        {
            auto cameraView = registry.View<TpsCameraComponent>();
            if(!cameraView.empty())
                cameraYaw = cameraView.get<TpsCameraComponent>(cameraView.front()).yaw;
        }
        // yaw=0のとき-Z方向（カメラの後方）を基準とした球面座標に対応する、カメラの水平方向の前後・左右ベクトル
        hlslpp::float3 cameraForward = hlslpp::float3(-std::sin(cameraYaw), 0.0f, std::cos(cameraYaw));
        hlslpp::float3 cameraRight   = hlslpp::float3(std::cos(cameraYaw), 0.0f, std::sin(cameraYaw));

        //-------------------------------------------------------------
        // viewを取得して各プレイヤーを更新
        //-------------------------------------------------------------
        auto view = registry.View<Tsukino::BuiltIn::ECS::TransformComponent,
                                  PlayerComponent,
                                  Tsukino::BuiltIn::ECS::CharacterControllerComponent>();
        view.each([&](entt::entity                                    entity,
                      Tsukino::BuiltIn::ECS::TransformComponent&      transform,
                      PlayerComponent&                                player,
                      Tsukino::BuiltIn::ECS::CharacterControllerComponent& cc) {
            //-------------------------------------------------------------
            // 攻撃中か（前フレームにPlayerAnimationSystemが確定した値）を取得。
            // 攻撃モーション中は移動方向への向き直しを止める
            //-------------------------------------------------------------
            bool isAttacking = false;
            if(player.weaponEntity != entt::null && registry.HasComponent<WeaponComponent>(player.weaponEntity)) {
                isAttacking = registry.GetComponent<WeaponComponent>(player.weaponEntity).isAttacking;
            }

            //-------------------------------------------------------------
            // isAttackingはPlayerAnimationSystem（本Systemより後に実行される）が
            // 今フレーム確定させる値なので、ここではまだ前フレームの値のまま＝
            // 攻撃ボタンを押した瞬間のフレームだけ「攻撃中ではない」と誤判定してしまい、
            // その時点の移動入力へ向き直りが1フレーム分だけ紛れ込む
            // （攻撃開始時の向き、ひいてはハンマーの振り始めの向きが移動入力に左右されてしまう
            //   不具合の原因だった）。攻撃ボタンの入力自体はこのSystem内で直接見られるので、
            // 「今フレーム攻撃がトリガーされるか」も向き直り抑制条件に含めて1フレームの遅れを潰す
            //-------------------------------------------------------------
            bool attackTriggeredThisFrame = inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::LButton);

            //-------------------------------------------------------------
            // 回避も攻撃と全く同じ事情を持つ：PlayerComponent::isDodgingを確定させるのは
            // 本Systemより後に走るPlayerAnimationSystemなので、ここで読めるのは前フレームの値。
            // 回避を押した瞬間のフレームだけ向き直りが1フレーム分紛れ込み、
            // 前転の向きが最後の移動入力に引っ張られてしまうため、今フレームのスペース押下も
            // 抑制条件へ含める（回避の向きはPlayerAnimationSystemが開始時に一度だけ確定させる）
            //-------------------------------------------------------------
            bool dodgeTriggeredThisFrame = inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::Space);

            //-------------------------------------------------------------
            // 移動方向の入力を取得（カメラの向きを基準にしたXZ平面）
            //-------------------------------------------------------------
            hlslpp::float3 moveDir = hlslpp::float3(0.0f, 0.0f, 0.0f);

            if(inputSystem->IsKeyDown(Tsukino::Input::KeyCode::Up) || inputSystem->IsKeyDown(Tsukino::Input::KeyCode::W))
                moveDir = moveDir + cameraForward;
            if(inputSystem->IsKeyDown(Tsukino::Input::KeyCode::Down) || inputSystem->IsKeyDown(Tsukino::Input::KeyCode::S))
                moveDir = moveDir - cameraForward;
            if(inputSystem->IsKeyDown(Tsukino::Input::KeyCode::Right) || inputSystem->IsKeyDown(Tsukino::Input::KeyCode::D))
                moveDir = moveDir + cameraRight;
            if(inputSystem->IsKeyDown(Tsukino::Input::KeyCode::Left) || inputSystem->IsKeyDown(Tsukino::Input::KeyCode::A))
                moveDir = moveDir - cameraRight;

            float len = hlslpp::length(moveDir);
            if(len > 0.001f) {
                moveDir = moveDir / len;

                // Shift押下中はスプリント（PlayerAnimationSystemがFastRun状態の判定に使う）
                player.isSprinting  = inputSystem->IsKeyDown(Tsukino::Input::KeyCode::Shift);
                float currentSpeed = player.isSprinting ? player.moveSpeed * player.sprintSpeedMultiplier : player.moveSpeed;

                // CharacterControllerComponentへ水平方向の希望移動速度を渡す
                cc.moveInput = moveDir * currentSpeed;

                if(!isAttacking && !attackTriggeredThisFrame && !player.isDodging && !dodgeTriggeredThisFrame) {
                    // 移動方向へ向き直す（瞬時に向かず、slerpで滑らかに補間する）
                    float               yawRad         = std::atan2(moveDir.x, moveDir.z);
                    hlslpp::quaternion targetRotation = hlslpp::quaternion::rotation_y(yawRad);
                    float               turnT          = 1.0f - std::exp(-player.turnLerpSpeed * deltaTime);
                    // slerpの結果をtransform.rotationへ自己代入し続けると誤差が蓄積して非正規化する。
                    // このrotationはPhysicsSystemがJoltへ渡す（Kinematicセンサー同期・CharacterVirtual同期の
                    // 両方）ほか、worldMatrixの生成や武器のアタッチ計算でも読まれるため、
                    // 単位クォータニオンであることをここで保証しておく
                    transform.rotation                 = hlslpp::normalize(hlslpp::slerp(transform.rotation, targetRotation, turnT));
                    transform.dirty                     = true;
                }
            } else {
                player.isSprinting = false;
                cc.moveInput         = hlslpp::float3(0.0f, 0.0f, 0.0f);
            }

            //-------------------------------------------------------------
            // 接地している時のみ回避（前転）の生入力をプレイヤーへ伝える。
            // 実際に回避へ移れるか（クールダウン中でないか、攻撃モーション中でないか）の判定と、
            // 攻撃中に押された場合の先行入力バッファへの積み込みはPlayerAnimationSystemが行う
            //-------------------------------------------------------------
            if(cc.isGrounded && dodgeTriggeredThisFrame) {
                player.dodgeInputPressed = true;
            }

            //-------------------------------------------------------------
            // 左クリックの生入力をプレイヤーへ伝える。ここではまだ武器のattackRequestedは立てない：
            // 連撃中の入力は「次段へのキャンセル」を意味することがあり、その判定はコンボ窓を
            // 見ているPlayerAnimationSystemが行う（実際の当たり判定の有効化・タイマー管理はCombatSystemが行う）
            //-------------------------------------------------------------
            if(inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::LButton) &&
               player.weaponEntity != entt::null &&
               registry.HasComponent<WeaponComponent>(player.weaponEntity)) {
                player.attackInputPressed = true;
            }

            //-------------------------------------------------------------
            // マウスホイールで装備中の武器を切り替える（1ノッチ=±1.0）。
            // 攻撃中に切り替わると持ち替えの見た目が破綻するため、攻撃中は入力を無視する
            // （isAttacking/attackTriggeredThisFrameは上の向き直り抑制と同じ判定を流用）
            //-------------------------------------------------------------
            float wheelDelta = inputSystem->GetWheelDelta();
            if(!isAttacking && !attackTriggeredThisFrame &&
               wheelDelta != 0.0f && player.weaponInventory.size() > 1) {
                int direction   = wheelDelta > 0.0f ? 1 : -1;
                int weaponCount = static_cast<int>(player.weaponInventory.size());

                // 今まで選択していた武器の浮遊ブーストを解除してから、新しい武器へ差し替える
                if(registry.HasComponent<WeaponComponent>(player.weaponEntity))
                    registry.GetComponent<WeaponComponent>(player.weaponEntity).floatSelected = false;

                player.selectedWeaponIndex = (player.selectedWeaponIndex + direction + weaponCount) % weaponCount;
                player.weaponEntity         = player.weaponInventory[player.selectedWeaponIndex];

                if(registry.HasComponent<WeaponComponent>(player.weaponEntity))
                    registry.GetComponent<WeaponComponent>(player.weaponEntity).floatSelected = true;
            }
        });
    }
}    // namespace CombatAndroid::ECS
