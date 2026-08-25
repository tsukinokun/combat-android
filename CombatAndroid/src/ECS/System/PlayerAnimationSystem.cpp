//-------------------------------------------------------------
//! @file   PlayerAnimationSystem.cpp
//! @brief  PlayerAnimationSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PlayerAnimationSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Core/typedef.hpp>
#include <Tsukino/Core/Log.hpp>

#include <hlsl++.h>
#include <algorithm>
#include <cmath>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! @brief ステート切り替え時のデフォルトクロスフェード時間（秒）
        constexpr float kAnimBlendTime = 0.15f;

        //-------------------------------------------------------------
        //! @brief  「指定クリップへクロスフェードする」OnEnterコールバックを作るヘルパー
        //! @param  clipMember     [in] PlayerAnimationSetComponentが持つクリップハンドルへのメンバポインタ
        //! @param  animationIndex [in] AnimationControllerComponentへ渡す再生インデックス
        //! @param  looping        [in] ループ再生するか
        //! @param  fadeTime       [in] クロスフェードにかける時間（秒）
        //-------------------------------------------------------------
        StateMachine<PlayerAnimState>::Callback MakeClipEnterCallback(Tsukino::Asset::AssetHandle PlayerAnimationSetComponent::* clipMember,
                                                                        u32                                                        animationIndex,
                                                                        bool                                                       looping,
                                                                        float                                                      fadeTime = kAnimBlendTime,
                                                                        bool                                                       inPlace  = false) {
            return [clipMember, animationIndex, looping, fadeTime, inPlace](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
                Tsukino::Asset::AssetHandle clip = registry.GetComponent<PlayerAnimationSetComponent>(entity).*clipMember;
                if(!clip.IsValid())
                    return;

                auto& animController = registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationControllerComponent>(entity);

                animController.next.clip            = clip;
                animController.next.animation_index = animationIndex;
                animController.next.fade_time       = fadeTime;
                animController.next.immediate       = false;    // クロスフェードで切り替える
                animController.next.is_looping      = looping;
                animController.next.clip_start_time = 0.0f;     // クリップ全体を再生（部分再生はAttack1-3のみ使う）
                animController.next.clip_end_time   = 0.0f;
                animController.next.in_place        = inPlace;

                // 攻撃段のAttackStep::playbackSpeedが前段から持ち越されないよう、
                // 攻撃以外のステートへ入るときは必ず等速へ戻す
                registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity).playback_speed = 1.0f;
            };
        }

        //-------------------------------------------------------------
        //! @brief  「連撃のN段目へ入る」OnEnterコールバックを作るヘルパー
        //! @param  stepIndex [in] PlayerAnimationSetComponent::attackSteps のインデックス
        //-------------------------------------------------------------
        StateMachine<PlayerAnimState>::Callback MakeAttackStepEnterCallback(u32 stepIndex) {
            return [stepIndex](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
                auto&       animSet = registry.GetComponent<PlayerAnimationSetComponent>(entity);
                const auto& step    = animSet.attackSteps[stepIndex];
                if(!step.clip.IsValid())
                    return;

                auto& animController = registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationControllerComponent>(entity);

                animController.next.clip            = step.clip;
                animController.next.animation_index = step.animationIndex;
                animController.next.fade_time       = step.fadeTime;
                animController.next.immediate       = false;    // クロスフェードで切り替える
                animController.next.is_looping      = false;    // 各段は単発再生（コンボ窓を過ぎたら次段かIdle等へ抜ける）
                animController.next.clip_start_time = step.startTime;
                animController.next.clip_end_time   = step.endTime;
                animController.next.in_place        = step.inPlace;

                // この段の再生速度倍率を適用する（攻撃モーションの速さの微調整用）。
                // MakeClipEnterCallback側で他ステートに入るときに1.0へ戻すため、ここでの
                // 変更が移動/待機アニメーションへ漏れることはない
                registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity).playback_speed = step.playbackSpeed;

                animSet.attackComboIndex = stepIndex;
                animSet.attackTimer      = 0.0f;

                //-------------------------------------------------------------
                // 武器の当たり判定を再アームする。WeaponComponent::cooldown（単発攻撃前提の値）が
                // コンボ窓キャンセルのタイミングでまだ残っている可能性があるため、段の突入時に
                // 明示的に0へ落として次段のヒット判定をブロックしないようにする
                //-------------------------------------------------------------
                auto& player = registry.GetComponent<PlayerComponent>(entity);
                if(player.weaponEntity != entt::null && registry.HasComponent<WeaponComponent>(player.weaponEntity)) {
                    auto& weapon            = registry.GetComponent<WeaponComponent>(player.weaponEntity);
                    weapon.attackRequested = true;
                    weapon.cooldownTimer   = 0.0f;
                    weapon.nextActiveDurationOverride = step.hitWindowDuration;
                    weapon.damageMultiplier            = step.damageMultiplier;
                }
            };
        }

        //-------------------------------------------------------------
        //! @brief  0-basedの段インデックスを対応するPlayerAnimStateへ変換する
        //-------------------------------------------------------------
        PlayerAnimState AttackStateFromIndex(u32 index) {
            switch(index) {
            case 0: return PlayerAnimState::Attack1;
            case 1: return PlayerAnimState::Attack2;
            default: return PlayerAnimState::Attack3;
            }
        }

        //-------------------------------------------------------------
        //! @brief  指定ステートが連撃中（Attack1-3のいずれか）かを判定する
        //-------------------------------------------------------------
        bool IsAttackState(PlayerAnimState state) {
            return state == PlayerAnimState::Attack1 || state == PlayerAnimState::Attack2 || state == PlayerAnimState::Attack3;
        }

        //-------------------------------------------------------------
        //! @brief  回避の進行方向を決める。移動入力があればその方向、無ければキャラの正面を返す
        //! @param  moveInput [in] CharacterControllerComponent::moveInput（PlayerSystemが今フレーム書いた値。カメラ基準・水平）
        //! @param  rotation  [in] プレイヤーの現在の姿勢
        //! @return 水平・正規化済みの回避方向
        //-------------------------------------------------------------
        hlslpp::float3 ResolveDodgeDirection(const hlslpp::float3& moveInput, const hlslpp::quaternion& rotation) {
            hlslpp::float3 horizontal = hlslpp::float3(moveInput.x, 0.0f, moveInput.z);
            float          length     = hlslpp::length(horizontal);
            if(length > 0.001f)
                return horizontal / length;

            //-------------------------------------------------------------
            // 移動入力が無い場合はキャラの正面へ前転する。PlayerSystemが
            // atan2(moveDir.x, moveDir.z) + rotation_y で向きを作っているため、ローカル前方は+Z
            //-------------------------------------------------------------
            hlslpp::float3 forward = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), rotation);
            forward.y              = 0.0f;

            float forwardLength = hlslpp::length(forward);
            // 真上/真下を向いている等でXZ成分が消えた場合のフォールバック（通常は起きない）
            return forwardLength > 0.001f ? forward / forwardLength : hlslpp::float3(0.0f, 0.0f, 1.0f);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief コンストラクタ。各ステートのOnEnterコールバック（クリップ切り替え）を登録する
    //-------------------------------------------------------------
    PlayerAnimationSystem::PlayerAnimationSystem() {
        // これらのMixamo由来のFBXは、いずれもindex 0が「Armature」レイヤーの1tickのスタブ、
        // index 1が実際の全ボーンモーション（52チャンネル）になっているため、再生には1を指定する
        m_stateMachine.RegisterState(PlayerAnimState::Idle, MakeClipEnterCallback(&PlayerAnimationSetComponent::idleClip, 1, true));
        m_stateMachine.RegisterState(PlayerAnimState::Run, MakeClipEnterCallback(&PlayerAnimationSetComponent::runClip, 1, true));
        m_stateMachine.RegisterState(PlayerAnimState::FastRun, MakeClipEnterCallback(&PlayerAnimationSetComponent::fastRunClip, 1, true));
        // 回避（前転）は緊急動作なので入りのクロスフェードは短く。前進はCharacterControllerが
        // 担当する（Update側でmoveInputを上書きする）ため、クリップ側のルート移動はin_placeで殺す
        m_stateMachine.RegisterState(PlayerAnimState::Dodge, MakeClipEnterCallback(&PlayerAnimationSetComponent::dodgeClip, 1, false, 0.08f, true));
        // 連撃の各段は、Weapon Attack.fbx 1本を時間レンジで3分割して参照する（PlayerAnimationSetComponent::attackSteps）
        m_stateMachine.RegisterState(PlayerAnimState::Attack1, MakeAttackStepEnterCallback(0));
        m_stateMachine.RegisterState(PlayerAnimState::Attack2, MakeAttackStepEnterCallback(1));
        m_stateMachine.RegisterState(PlayerAnimState::Attack3, MakeAttackStepEnterCallback(2));
        // 死亡モーションは単発再生・ルート前進を殺す（in_place）。倒れた後も画面内に留まらせるため
        m_stateMachine.RegisterState(PlayerAnimState::Death, MakeClipEnterCallback(&PlayerAnimationSetComponent::deathClip, 1, false, 0.10f, true));
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PlayerAnimationSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto view = registry.View<PlayerComponent,
                                  Tsukino::BuiltIn::ECS::CharacterControllerComponent,
                                  PlayerAnimationSetComponent,
                                  Tsukino::BuiltIn::ECS::AnimationPlayerComponent,
                                  Tsukino::BuiltIn::ECS::TransformComponent,
                                  HealthComponent>();
        view.each([&](entt::entity                                          entity,
                     PlayerComponent&                                      player,
                     Tsukino::BuiltIn::ECS::CharacterControllerComponent& cc,
                     PlayerAnimationSetComponent&                          animSet,
                     Tsukino::BuiltIn::ECS::AnimationPlayerComponent&     animPlayer,
                     Tsukino::BuiltIn::ECS::TransformComponent&            transform,
                     HealthComponent&                                     health) {
            //-------------------------------------------------------------
            // 死亡時は他の全判定より最優先でDeathへ遷移し、操作を止める。
            // 以後の攻撃・回避・移動の判定は一切評価しない
            //-------------------------------------------------------------
            if(health.isDead) {
                cc.moveInput               = hlslpp::float3(0.0f, 0.0f, 0.0f);
                player.attackInputPressed = false;
                player.dodgeInputPressed  = false;
                player.isDodging           = false;
                player.isInvincible        = false;

                if(player.weaponEntity != entt::null && registry.HasComponent<WeaponComponent>(player.weaponEntity))
                    registry.GetComponent<WeaponComponent>(player.weaponEntity).isAttacking = false;

                m_stateMachine.TransitionTo(animSet.currentState, PlayerAnimState::Death, registry, entity);
                return;
            }

            bool isAttacking = IsAttackState(animSet.currentState);
            bool isDodging   = animSet.currentState == PlayerAnimState::Dodge;

            if(isAttacking) {
                animSet.attackTimer += deltaTime;
            }

            if(isDodging) {
                animSet.dodgeTimer += deltaTime;
            } else if(animSet.dodgeCooldownTimer > 0.0f) {
                // クールダウンは回避を抜けた後にのみ進める
                animSet.dodgeCooldownTimer = std::max(animSet.dodgeCooldownTimer - deltaTime, 0.0f);
            }

            //-------------------------------------------------------------
            // 左クリック／スペースの生入力（PlayerComponent::attackInputPressed／dodgeInputPressed）を
            // 消費する。モーション再生中でなければ即座に開始し、再生中なら1つだけ先行入力として
            // バッファする（連打しても2段先へは飛ばない）。バッファは攻撃・回避で共用し、
            // 後から押された方で上書きする＝最新の入力が勝つ。
            // バッファされた入力は現在のモーションを最後まで再生し終えてから消費するため、
            // 再生中のモーションが途中でキャンセルされることはない
            //-------------------------------------------------------------
            bool attackJustPressed     = player.attackInputPressed;
            player.attackInputPressed = false;

            bool dodgeJustPressed    = player.dodgeInputPressed;
            player.dodgeInputPressed = false;

            bool hasWeapon = player.weaponEntity != entt::null && registry.HasComponent<WeaponComponent>(player.weaponEntity);

            bool attackJustStarted = false;
            if(attackJustPressed && hasWeapon) {
                if(!isAttacking && !isDodging) {
                    attackJustStarted = true;
                } else {
                    // 回避中の攻撃入力もここへ入り、回避が終わってから1段目として消費される
                    animSet.bufferedInput = BufferedInput::Attack;
                }
            }

            bool dodgeJustStarted = false;
            if(dodgeJustPressed) {
                if(!isAttacking && !isDodging) {
                    // クールダウン中の入力は捨てる（バッファすると、待っただけで
                    // 溜めていた回避が暴発しているように見えてしまう）
                    dodgeJustStarted = animSet.dodgeCooldownTimer <= 0.0f;
                } else if(isAttacking) {
                    animSet.bufferedInput = BufferedInput::Dodge;
                }
                // 回避中の回避入力は無視する（抜けた直後は必ずクールダウン中で弾かれるため、バッファしない）
            }

            //-------------------------------------------------------------
            // 現在の段の終了判定は、原則としてAnimationPlayerComponent::is_finished
            // （実クリップの再生完了）で行う。attackTimeoutSafetyはクリップ設定ミス等の保険
            // （詳細はPlayerAnimationSetComponent::attackTimeoutSafetyのコメント参照）
            //-------------------------------------------------------------
            bool attackStepFinished = isAttacking && (animPlayer.is_finished || animSet.attackTimer >= animSet.attackTimeoutSafety);

            //-------------------------------------------------------------
            // 回避の終了判定も攻撃段と同じ流儀（実クリップの再生完了＋保険のタイムアウト）で行う
            //-------------------------------------------------------------
            bool dodgeFinished = isDodging && (animPlayer.is_finished || animSet.dodgeTimer >= player.dodgeTimeoutSafety);

            //-------------------------------------------------------------
            // 現在のプレイヤーの状態から、あるべきアニメーションステートを決定する
            // （移動の判定はまず素で計算し、回避・攻撃の継続/開始/コンボ進行が
            //   優先される場合はその後で上書きする）。
            // 接地判定による分岐は持たない：ジャンプを廃止したため、落下中（開始直後の
            // 着地待ちや段差）も移動判定に従ってIdle/Runのままでよい
            //-------------------------------------------------------------
            float moveInputLen = hlslpp::length(cc.moveInput);

            PlayerAnimState desiredState;
            if(moveInputLen > 1.0f) {
                desiredState = player.isSprinting ? PlayerAnimState::FastRun : PlayerAnimState::Run;
            } else {
                desiredState = PlayerAnimState::Idle;
            }

            //-------------------------------------------------------------
            // 回避へ入る。方向はこの時点のcc.moveInput（PlayerSystemが今フレーム書いた
            // カメラ基準の移動入力）から決め、以後は入力に関わらずその方向へ進む。
            // 向きも開始時に一度だけ合わせる（PlayerSystem側は回避中の向き直りを抑制している）
            //-------------------------------------------------------------
            auto EnterDodge = [&]() {
                animSet.dodgeTimer     = 0.0f;
                animSet.dodgeDirection = ResolveDodgeDirection(cc.moveInput, transform.rotation);

                float yawRad       = std::atan2(animSet.dodgeDirection.x, animSet.dodgeDirection.z);
                transform.rotation = hlslpp::quaternion::rotation_y(yawRad);
                transform.dirty    = true;
            };

            if(dodgeJustStarted) {
                // 回避は最優先。ただし攻撃中・回避中は上のdodgeJustStartedが立たないため、
                // ここへ来るのは「何もしていない状態から回避を始める」場合だけ
                EnterDodge();
                desiredState = PlayerAnimState::Dodge;
            } else if(attackJustStarted) {
                desiredState = PlayerAnimState::Attack1;
            } else if(isDodging && !dodgeFinished) {
                desiredState = animSet.currentState;    // 回避は最後まで再生する（キャンセル不可）
            } else if(dodgeFinished) {
                animSet.dodgeCooldownTimer = player.dodgeCooldown;

                if(animSet.bufferedInput == BufferedInput::Attack && hasWeapon) {
                    // 回避中に押された攻撃をここで消費して連撃の1段目へ入る
                    desiredState = PlayerAnimState::Attack1;
                }
                // それ以外はdesiredStateを上で計算した移動判定のまま使う
                animSet.bufferedInput = BufferedInput::None;
            } else if(isAttacking && !attackStepFinished) {
                desiredState = animSet.currentState;    // 現在の段を最後まで再生する（キャンセルしない）
            } else if(attackStepFinished) {
                bool isLastStep = animSet.attackComboIndex + 1 >= PlayerAnimationSetComponent::kAttackComboCount;

                if(animSet.bufferedInput == BufferedInput::Dodge && animSet.dodgeCooldownTimer <= 0.0f) {
                    //-------------------------------------------------------------
                    // 攻撃中に押されたスペースをここで消費する。攻撃モーションはキャンセルせず、
                    // 段の再生を終えてから回避へ移る（＝押しっぱなしの緊急回避ではなく「予約」）。
                    // 攻撃の後にさらに左クリックを押していればbufferedInputはAttackで上書き
                    // されているため、その場合はこの分岐へ来ず連撃が継続する
                    //-------------------------------------------------------------
                    animSet.bufferedInput    = BufferedInput::None;
                    animSet.attackComboIndex = 0;
                    EnterDodge();
                    desiredState = PlayerAnimState::Dodge;
                } else if(animSet.bufferedInput == BufferedInput::Attack && !isLastStep) {
                    // 先行入力があれば次段へ連撃継続する
                    animSet.bufferedInput = BufferedInput::None;
                    u32 nextComboIndex     = animSet.attackComboIndex + 1;
                    desiredState           = AttackStateFromIndex(nextComboIndex);
                } else {
                    // コンボをリセットする。desiredStateは上で計算した移動判定のまま使う。
                    // 3段目が終わった直後は、先行入力が残っていても連撃を継続させず一度破棄する
                    // （3段目終了＝一区切りとして、必ず新しい入力から次のコンボを始めさせる）
                    animSet.attackComboIndex = 0;
                    animSet.bufferedInput     = BufferedInput::None;
                }
            }

            bool willBeAttacking = IsAttackState(desiredState);
            bool willBeDodging   = desiredState == PlayerAnimState::Dodge;

            //-------------------------------------------------------------
            // 攻撃中は移動させない（PlayerSystemが今フレーム書き込んだ
            // moveInputを、Physicsが消費する前にここで打ち消す）。
            // 逆に回避中は、開始時に確定した方向へ一定速度で進ませる
            // （エンジンはクリップのルートモーションをTransformへ適用しないため、
            //   前転の前進はここでCharacterControllerへ与える必要がある）
            //-------------------------------------------------------------
            if(willBeAttacking) {
                cc.moveInput = hlslpp::float3(0.0f, 0.0f, 0.0f);
            } else if(willBeDodging) {
                cc.moveInput = animSet.dodgeDirection * player.dodgeSpeed;
            }

            //-------------------------------------------------------------
            // ステートマシンに遷移を委譲する（変化がなければ何もしない。
            // 変化していればOnExit→OnEnterの順でコールバックが呼ばれ、クリップが切り替わる）
            //-------------------------------------------------------------
            m_stateMachine.TransitionTo(animSet.currentState, desiredState, registry, entity);

            //-------------------------------------------------------------
            // 回避クリップの再生速度を適用する。MakeClipEnterCallbackはOnEnterで
            // playback_speedを1.0へ戻す（攻撃段の倍率を持ち越さないため）ので、
            // それより後であるここで毎フレーム上書きする。回避を抜けたフレームでは
            // 次ステートのOnEnterが1.0へ戻すため、倍率が他のモーションへ漏れることはない
            //-------------------------------------------------------------
            if(willBeDodging) {
                animPlayer.playback_speed = player.dodgePlaybackSpeed;
            }

            //-------------------------------------------------------------
            // 装備中の武器へ「攻撃アニメーション再生中か」を伝える
            // （CombatSystemがこれを見て、浮遊演出のON/OFFと手ボーンへの追従度を切り替える）
            //-------------------------------------------------------------
            if(hasWeapon) {
                registry.GetComponent<WeaponComponent>(player.weaponEntity).isAttacking = willBeAttacking;
            }

            //-------------------------------------------------------------
            // 回避の状態を他Systemへ公開する。
            // isDodging   … PlayerSystem（次フレーム）が向き直りの抑制に読む
            // isInvincible… CombatSystem（本Systemより後に走る）が接触ダメージのスキップに読む。
            //               無敵は回避の前半だけなので、終わり際に敵と重なったままなら被弾する
            //-------------------------------------------------------------
            player.isDodging    = willBeDodging;
            player.isInvincible = willBeDodging && animSet.dodgeTimer < player.dodgeInvincibleDuration;

#if defined(_DEBUG)
            //-------------------------------------------------------------
            // 連撃の分割点調整用ログ。0.1秒間隔に間引いて出す。
            // WeaponGripDebugSystemのF10（ポーズ固定）/F11（1コマ送り）と併用し、
            // フレーム番号を見ながらCombatAndroidScene.cppの分割定数を追い込む
            //-------------------------------------------------------------
            if(willBeAttacking) {
                static float debugLogTimer = 0.0f;
                debugLogTimer              += deltaTime;
                if(debugLogTimer > 0.1f) {
                    debugLogTimer = 0.0f;

                    const auto& step       = animSet.attackSteps[animSet.attackComboIndex];
                    float       stepLength = std::max(step.endTime - step.startTime, 0.0001f);
                    // animPlayer.elapsed_timeはこの段のクリップレンジ先頭からの経過秒（playbackSpeed適用済み）。
                    // 段自身の経過を別途real timeで数えるより、実際に進んだクリップ位置を直接見る方が
                    // playbackSpeedを変えたときもズレない
                    float       normalized = animPlayer.elapsed_time / stepLength;
                    // ticksPerSecondを持たないためここでは30fps相当で概算する（実クリップのfpsとずれる場合がある点に注意）
                    float       clipFrame  = (step.startTime + animPlayer.elapsed_time) * 30.0f;

                    Tsukino::Core::Log::Info(
                        "ATTACK step=" + std::to_string(animSet.attackComboIndex + 1) + "/"
                        + std::to_string(PlayerAnimationSetComponent::kAttackComboCount)
                        + "  frame=" + std::to_string(clipFrame)
                        + " (clip " + std::to_string(step.startTime) + "-" + std::to_string(step.endTime) + "s)"
                        + "  speed=" + std::to_string(step.playbackSpeed)
                        + "  normalized=" + std::to_string(normalized)
                        + "  buffered=" + std::to_string(static_cast<int>(animSet.bufferedInput)));
                }
            }

            //-------------------------------------------------------------
            // 回避のチューニング用ログ（PlayerComponent::dodgeInvincibleDuration の追い込み用）。
            // ATTACKログと同じく0.1秒間隔に間引いて出す
            //-------------------------------------------------------------
            if(willBeDodging) {
                static float dodgeLogTimer = 0.0f;
                dodgeLogTimer              += deltaTime;
                if(dodgeLogTimer > 0.1f) {
                    dodgeLogTimer = 0.0f;

                    Tsukino::Core::Log::Info(
                        "DODGE t=" + std::to_string(animSet.dodgeTimer)
                        + "  invincible=" + std::to_string(player.isInvincible ? 1 : 0)
                        + " (<" + std::to_string(player.dodgeInvincibleDuration) + "s)"
                        + "  dir=(" + std::to_string(animSet.dodgeDirection.x) + ", " + std::to_string(animSet.dodgeDirection.z) + ")"
                        + "  buffered=" + std::to_string(static_cast<int>(animSet.bufferedInput)));
                }
            }
#endif
        });
    }
}    // namespace CombatAndroid::ECS
