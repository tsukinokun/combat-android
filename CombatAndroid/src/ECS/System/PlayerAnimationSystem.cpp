//-------------------------------------------------------------
//! @file   PlayerAnimationSystem.cpp
//! @brief  PlayerAnimationSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PlayerAnimationSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/HitStopComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/HighlightComponent.hpp>

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
        //! @param  stepIndex  [in] PlayerAnimationSetComponent::attackSteps のインデックス
        //! @param  armAttack  [in] 当たり判定をアームするか。falseの場合はクリップ再生のみ行い、
        //!                          WeaponComponent::attackRequested等は一切変更しない（Chargeステートで使う）
        //-------------------------------------------------------------
        StateMachine<PlayerAnimState>::Callback MakeAttackStepEnterCallback(u32 stepIndex, bool armAttack = true) {
            return [stepIndex, armAttack](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
                auto&       animSet = registry.GetComponent<PlayerAnimationSetComponent>(entity);
                const auto& step    = animSet.attackSteps[stepIndex];

                //-------------------------------------------------------------
                // 再生するクリップは既定でプレイヤー側のstep（Hammer Attack.fbx）を使う。
                // 装備武器がWeaponComponent::attackClipを持つ場合のみ、その武器専用の
                // クリップ・時間レンジへ差し替える（武器種別を判定するenumは持たず、
                // areaAttack等と同じくWeaponComponent側のオプトインフィールドで判定する）
                //-------------------------------------------------------------
                Tsukino::Asset::AssetHandle clip           = step.clip;
                u32                          animationIndex = step.animationIndex;
                float                        startTime      = step.startTime;
                float                        endTime        = step.endTime;

                auto& player = registry.GetComponent<PlayerComponent>(entity);
                WeaponComponent* weapon = nullptr;
                if(player.weaponEntity != entt::null && registry.HasComponent<WeaponComponent>(player.weaponEntity))
                    weapon = &registry.GetComponent<WeaponComponent>(player.weaponEntity);

                if(weapon != nullptr && weapon->attackClip.IsValid()) {
                    clip           = weapon->attackClip;
                    animationIndex = weapon->attackAnimationIndex;
                    startTime      = weapon->attackStepStartTime[stepIndex];
                    endTime        = weapon->attackStepEndTime[stepIndex];
                }

                if(!clip.IsValid())
                    return;

                //-------------------------------------------------------------
                // 通常は毎回このクリップ・レンジを頭から再生し直す（AnimationControllerComponent::next
                // を発行するとAnimationSystemがelapsed_timeを0へリセットするため）。ただし
                // suppressAnimationRestartが立っている場合（溜め攻撃の解放）は、同じクリップ・
                // レンジを既にChargeステートで再生中なので、nextの発行自体をスキップして現在の
                // 再生（elapsed_time含む）をそのまま引き継ぐ＝「溜めていたポーズの続き」から再生する
                //-------------------------------------------------------------
                if(animSet.suppressAnimationRestart) {
                    animSet.suppressAnimationRestart = false;    // 一度きりの消費
                } else {
                    auto& animController = registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationControllerComponent>(entity);

                    animController.next.clip            = clip;
                    animController.next.animation_index = animationIndex;
                    animController.next.fade_time       = step.fadeTime;
                    animController.next.immediate       = false;    // クロスフェードで切り替える
                    animController.next.is_looping      = false;    // 各段は単発再生（コンボ窓を過ぎたら次段かIdle等へ抜ける）
                    animController.next.clip_start_time = startTime;
                    animController.next.clip_end_time   = endTime;
                    animController.next.in_place        = step.inPlace;
                }

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
                if(armAttack && weapon != nullptr) {
                    weapon->attackRequested = true;
                    weapon->cooldownTimer   = 0.0f;
                    weapon->nextActiveDurationOverride = step.hitWindowDuration;
                    weapon->damageMultiplier            = step.damageMultiplier;
                    // AoE(範囲攻撃)要求。実際に発動するかはCombatSystem側でweapon.areaAttackRadius>0を見て判定する
                    weapon->pendingAreaAttack      = step.areaAttack;
                    weapon->pendingAreaAttackDelay = step.areaAttackDelay;
                    // 斬撃弾は溜め攻撃の解放でしか撃たない。通常の段へ入るときは必ず下ろしておき、
                    // 前回の解放で立てた要求が持ち越されないようにする（この直後に走る
                    // 解放判定のブロックが、解放時だけ改めて立て直す）
                    weapon->pendingProjectile = false;
                }
            };
        }

        //-------------------------------------------------------------
        //! @brief  溜め攻撃（Chargeステート）を抜けるときのOnExitコールバック。
        //!         解放・回避によるキャンセル・死亡のいずれの経路でも必ず呼ばれるため、
        //!         ここでリムライトを消すだけで消灯漏れが起きない
        //-------------------------------------------------------------
        StateMachine<PlayerAnimState>::Callback MakeChargeExitCallback() {
            return [](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
                if(registry.HasComponent<Tsukino::BuiltIn::ECS::HighlightComponent>(entity))
                    registry.GetComponent<Tsukino::BuiltIn::ECS::HighlightComponent>(entity).active = false;
            };
        }

        //-------------------------------------------------------------
        //! @brief  溜め時間から現在の溜め段階（1=白, 2=青, 3=紫）を求める
        //-------------------------------------------------------------
        int ResolveChargeStage(float chargeTimer, const PlayerComponent& player) {
            if(chargeTimer >= player.chargeStage3Threshold) return 3;
            if(chargeTimer >= player.chargeStage2Threshold) return 2;
            return 1;
        }

        //-------------------------------------------------------------
        //! @brief  溜め段階から解放時のダメージ倍率を求める
        //-------------------------------------------------------------
        float ResolveChargeDamageMultiplier(int stage, const PlayerComponent& player) {
            switch(stage) {
            case 3:  return player.chargeDamageMultiplierStage3;
            case 2:  return player.chargeDamageMultiplierStage2;
            default: return player.chargeDamageMultiplierStage1;
            }
        }

        //-------------------------------------------------------------
        //! @brief  溜め段階からリムライトの色を求める（白→青→紫）
        //-------------------------------------------------------------
        hlslpp::float3 ResolveChargeRimColor(int stage) {
            switch(stage) {
            case 3:  return hlslpp::float3(0.65f, 0.15f, 1.0f);    // 紫
            case 2:  return hlslpp::float3(0.25f, 0.55f, 1.0f);    // 青
            default: return hlslpp::float3(1.0f, 1.0f, 1.0f);      // 白
            }
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
        // 溜め攻撃の構え。専用クリップは持たず、装備武器のAttack1相当のクリップ・時間レンジ（＝振りの
        // 前半部分）をそのまま使い、Update側で超スロー再生することで「振りかぶって溜めている」見た目にする。
        // armAttack=falseなので当たり判定はアームされない（発生するのは解放後のAttack1のみ）。
        // OnExitでリムライトを必ず消す（解放・回避キャンセル・死亡のどの経路でも呼ばれる）
        m_stateMachine.RegisterState(PlayerAnimState::Charge, MakeAttackStepEnterCallback(0, /*armAttack=*/false), MakeChargeExitCallback());
        // 連撃の各段は、Hammer Attack.fbx 1本を時間レンジで3分割して参照する（PlayerAnimationSetComponent::attackSteps）。
        // 装備武器がWeaponComponent::attackClipを持つ場合はそちらへ差し替わる（Great Sword等。MakeAttackStepEnterCallback参照）
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
        //-------------------------------------------------------------
        // スキル選択メニュー中は何もしない。攻撃中・回避中はここがCharacterControllerComponent::moveInput
        // を上書きするため、SkillSelectSystemが打ち消した値が戻ってしまう
        // （PhysicsSystemはdeltaTime=0でも1/60秒ぶん進むので、そのまま滑り続ける）。
        // 併せて、先行入力バッファの消費もメニュー中は止める
        //-------------------------------------------------------------
        if(IsSkillSelectActive(registry))
            return;

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
                player.isCharging          = false;    // Chargeで死亡した場合もOnExitがリムライトを消すため、ここでは公開フラグだけ落とす

                if(player.weaponEntity != entt::null && registry.HasComponent<WeaponComponent>(player.weaponEntity))
                    registry.GetComponent<WeaponComponent>(player.weaponEntity).isAttacking = false;

                m_stateMachine.TransitionTo(animSet.currentState, PlayerAnimState::Death, registry, entity);
                return;
            }

            bool isAttacking = IsAttackState(animSet.currentState);
            bool isDodging   = animSet.currentState == PlayerAnimState::Dodge;
            bool isCharging  = animSet.currentState == PlayerAnimState::Charge;

            if(isAttacking) {
                animSet.attackTimer += deltaTime;
            }

            if(isCharging) {
                animSet.chargeTimer += deltaTime;
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

            //-------------------------------------------------------------
            // 装備武器が溜め攻撃対応（WeaponComponent::chargeAttackEnabled）かどうかで、
            // 左クリックの押下を「即座にAttack1」か「Chargeへ入って構える」かに振り分ける
            //-------------------------------------------------------------
            bool weaponChargeCapable = hasWeapon && registry.GetComponent<WeaponComponent>(player.weaponEntity).chargeAttackEnabled;

            bool chargeJustStarted = false;
            bool attackJustStarted = false;
            if(attackJustPressed && hasWeapon) {
                if(!isAttacking && !isDodging && !isCharging) {
                    if(weaponChargeCapable) {
                        chargeJustStarted = true;
                    } else {
                        attackJustStarted = true;
                    }
                } else if(!isCharging) {
                    // 回避中・攻撃中の攻撃入力はここへ入り、モーションが終わってから1段目として消費される。
                    // 溜め中（isCharging）の再押下は起こり得ない（離すまでIsKeyPressedは再発火しない）ため無視する
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
            // 溜め攻撃の解放判定。左クリックを離すか、chargeMaxDurationに達したら強制的に解放する。
            // chargeStageは解放時のダメージ倍率・リムライト色の両方に使う（このフレームの溜め時間から算出）
            //-------------------------------------------------------------
            bool chargeReleased = isCharging && (!player.attackInputHeld || animSet.chargeTimer >= player.chargeMaxDuration);
            int  chargeStage     = isCharging ? ResolveChargeStage(animSet.chargeTimer, player) : 1;

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
                // 回避は最優先。ただし攻撃中・回避中・溜め中は上のdodgeJustStartedが立たないため、
                // ここへ来るのは「何もしていない状態から回避を始める」場合だけ……のはずだが、
                // 溜め中（isCharging）はisAttacking扱いではないため、溜め中に回避を押すとここへ来て
                // 攻撃を出さずに溜めをキャンセルして回避へ割り込める（Chargeのstate自体はOnExitで
                // リムライトを消すだけなので、キャンセルしても後始末は自動的に行われる）
                EnterDodge();
                desiredState = PlayerAnimState::Dodge;
            } else if(chargeJustStarted) {
                animSet.chargeTimer = 0.0f;
                desiredState         = PlayerAnimState::Charge;
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
            } else if(isCharging && !chargeReleased) {
                desiredState = animSet.currentState;    // 離す/強制タイムアウトまで構えを維持する
            } else if(isCharging && chargeReleased) {
                desiredState = PlayerAnimState::Attack1;    // 解放。1段目のスイングとして繰り出す
                // 溜めていたポーズの続きから再生する（頭からやり直さない）。Attack1のOnEnterが消費する
                animSet.suppressAnimationRestart = true;
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
            bool willBeCharging  = desiredState == PlayerAnimState::Charge;

            //-------------------------------------------------------------
            // 攻撃中・溜め攻撃中は移動させない（PlayerSystemが今フレーム書き込んだ
            // moveInputを、Physicsが消費する前にここで打ち消す）。
            // 逆に回避中は、開始時に確定した方向へ一定速度で進ませる
            // （エンジンはクリップのルートモーションをTransformへ適用しないため、
            //   前転の前進はここでCharacterControllerへ与える必要がある）
            //-------------------------------------------------------------
            if(willBeAttacking || willBeCharging) {
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
            // 溜め攻撃中の再生速度を超スローへ上書きする（Dodgeの再生速度上書きと同じ手法）。
            // MakeAttackStepEnterCallback（Chargeの OnEnter）が入り口で通常速度を設定するため、
            // それより後であるここで毎フレーム上書きする
            //-------------------------------------------------------------
            if(willBeCharging) {
                animPlayer.playback_speed = player.chargePlaybackSpeed;
            }

            //-------------------------------------------------------------
            // 装備中の武器へ「攻撃アニメーション再生中か」を伝える
            // （CombatSystemがこれを見て、浮遊演出のON/OFFと手ボーンへの追従度を切り替える）。
            // 溜め攻撃中も武器を構えたまま静止させたいため、攻撃中と同じ扱いにする
            //-------------------------------------------------------------
            if(hasWeapon) {
                registry.GetComponent<WeaponComponent>(player.weaponEntity).isAttacking = willBeAttacking || willBeCharging;
            }

            //-------------------------------------------------------------
            // 溜め攻撃を解放した瞬間、到達した段階に応じてダメージ倍率を上書きする。
            // 直前のTransitionToでAttack1のOnEnter（MakeAttackStepEnterCallback）が
            // damageMultiplier = step.damageMultiplier（既定1.0）を設定済みなので、それに乗算する
            //-------------------------------------------------------------
            if(isCharging && chargeReleased && hasWeapon) {
                WeaponComponent& releasedWeapon = registry.GetComponent<WeaponComponent>(player.weaponEntity);

                releasedWeapon.damageMultiplier *= ResolveChargeDamageMultiplier(chargeStage, player);
                // 解放直後のスイング（Attack1）は、Attack1のOnEnterが設定した通常速度よりも
                // 速く振らせる。段の速度（step.playbackSpeed）へ乗算する形にしておき、
                // 将来step側の速度を変えてもここが二重に効かないようにする
                animPlayer.playback_speed *= player.chargeReleasePlaybackSpeedMultiplier;

                // 前方へ飛ぶ斬撃弾を要求する。実際に撃つかはCombatSystem側で
                // weapon.projectileEffectAssetの有無を見て判定する（battleaxeのみ有効）。
                // 溜め段階の倍率は上でdamageMultiplierへ乗せてあるため、弾の威力にもそのまま乗る。
                // 段階そのものも渡すのは、貫通するかの判定（projectilePierceMinChargeStage）に要るため
                releasedWeapon.pendingProjectile           = true;
                releasedWeapon.pendingProjectileChargeStage = chargeStage;
            }

            //-------------------------------------------------------------
            // 溜め中（超スロー）・回避中（早回し）の再生速度は、このSystemが毎フレーム
            // 上書きして持っている「一時的な値」である。この最中に被弾すると、
            // ヒットストップ（HitStopComponent::baseAnimSpeed）がその一時的な値を
            // 「元の速度」として記録してしまい、ヒットストップ終了時にそれが復元される。
            // 溜めを抜けた後もchargePlaybackSpeed（0.15）のまま再生され続け、次にステートが
            // 切り替わる（＝OnEnterが等速へ戻す）までモーション全体がスローになる不具合の原因。
            //
            // そこで上書きを手放したこのフレームで、基準値を「今フレーム確定した速度」へ
            // 取り直す。ここに置いているのは、上の解放処理まで含めて今フレームの
            // playback_speedが確定した後だからである
            //-------------------------------------------------------------
            bool speedOverrideEnded = (isCharging && !willBeCharging) || (isDodging && !willBeDodging);
            if(speedOverrideEnded) {
                if(auto* hitStop = registry.try_get<HitStopComponent>(entity))
                    hitStop->baseAnimSpeed = animPlayer.playback_speed;
            }

            //-------------------------------------------------------------
            // 溜め攻撃中のリムライト点灯（白→青→紫）。消灯はChargeのOnExitコールバックが担当するため、
            // ここでは点灯中の値の書き込みのみ行う
            //-------------------------------------------------------------
            if(willBeCharging && registry.HasComponent<Tsukino::BuiltIn::ECS::HighlightComponent>(entity)) {
                auto& highlight         = registry.GetComponent<Tsukino::BuiltIn::ECS::HighlightComponent>(entity);
                highlight.active       = true;
                highlight.rimColor     = ResolveChargeRimColor(ResolveChargeStage(animSet.chargeTimer, player));
                highlight.rimIntensity = 5.0f;
                highlight.rimPower     = 2.5f;
                highlight.glow         = 0.3f;
            }

            //-------------------------------------------------------------
            // 回避の状態を他Systemへ公開する。
            // isDodging   … PlayerSystem（次フレーム）が向き直りの抑制に読む
            // isInvincible… CombatSystem（本Systemより後に走る）が接触ダメージのスキップに読む。
            //               無敵は回避の前半だけなので、終わり際に敵と重なったままなら被弾する
            //-------------------------------------------------------------
            player.isDodging    = willBeDodging;
            player.isInvincible = willBeDodging && animSet.dodgeTimer < player.dodgeInvincibleDuration;
            player.isCharging    = willBeCharging;    // PlayerSystem（次フレーム）が向き直り・武器切り替えの抑制に読む

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
