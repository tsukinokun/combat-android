//-------------------------------------------------------------
//! @file   EnemyAnimationSystem.cpp
//! @brief  EnemyAnimationSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/EnemyAnimationSystem.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyHeldWeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>

#include <entt/entt.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>

#include <Tsukino/Core/typedef.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kAnimBlendTime      = 0.15f;    //!< Idle/Walkへ切り替える際のクロスフェード時間（秒）
        constexpr float kAttackBlendTime    = 0.10f;    //!< Attackへ切り替える際のクロスフェード時間（秒。素早く反応させるため短め）
        constexpr float kKnockbackBlendTime = 0.06f;    //!< Knockbackへ切り替える際のクロスフェード時間（秒。被弾の反応は最速で入れたい）
        constexpr float kDeathBlendTime     = 0.10f;    //!< Deathへ切り替える際のクロスフェード時間（秒）

        //-------------------------------------------------------------
        //! @brief  「指定クリップへクロスフェードする」OnEnterコールバックを作るヘルパー
        //!         （PlayerAnimationSystem::MakeClipEnterCallbackと同じ考え方）
        //! @param  clipMember [in] EnemyAnimationSetComponentが持つクリップハンドルへのメンバポインタ
        //! @param  looping    [in] ループ再生するか
        //! @param  fadeTime   [in] クロスフェードにかける時間（秒）
        //! @param  inPlace    [in] ルートモーションの水平成分を殺すか
        //-------------------------------------------------------------
        StateMachine<EnemyAnimState>::Callback MakeClipEnterCallback(Tsukino::Asset::AssetHandle EnemyAnimationSetComponent::* clipMember,
                                                                        bool                                                     looping,
                                                                        float                                                    fadeTime,
                                                                        bool                                                     inPlace) {
            return [clipMember, looping, fadeTime, inPlace](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
                auto&                        animSet = registry.GetComponent<EnemyAnimationSetComponent>(entity);
                Tsukino::Asset::AssetHandle clip     = animSet.*clipMember;
                if(!clip.IsValid())
                    return;

                auto& animController = registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationControllerComponent>(entity);

                animController.next.clip            = clip;
                animController.next.animation_index = animSet.animationIndex;
                animController.next.fade_time       = fadeTime;
                animController.next.immediate       = false;    // クロスフェードで切り替える
                animController.next.is_looping      = looping;
                animController.next.clip_start_time = 0.0f;     // クリップ全体を再生する
                animController.next.clip_end_time   = 0.0f;
                animController.next.in_place        = inPlace;

                registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity).playback_speed = 1.0f;
            };
        }

        //-------------------------------------------------------------
        //! @brief  敵が手に持つ武器へ「攻撃アニメーション再生中か」を伝えるヘルパー
        //! @param  registry    [in] エンティティレジストリ
        //! @param  entity      [in] 敵本体のエンティティ
        //! @param  isAttacking [in] 攻撃アニメーション再生中か
        //! @note   CombatSystemはこのフラグからattackBlendを作り、非攻撃時のルート追従（浮遊）と
        //!         攻撃中の手ボーン追従を連続的に補間する。プレイヤー側でPlayerAnimationSystemが
        //!         WeaponComponent::isAttackingを書いているのとまったく同じ経路で、
        //!         本Systemも実行順が同じ（SystemPriority::Gameplay）ためCombatSystem
        //!         （SystemPriority::WeaponAttach）が同じフレームのうちに読める。
        //!         武器は敵の子ではなく独立したエンティティなのでEnemyHeldWeaponComponent越しに引く。
        //!         コンポーネント構成は変えずフィールドを書くだけなので、View反復中に呼んでも安全
        //-------------------------------------------------------------
        void SetHeldWeaponAttacking(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, bool isAttacking) {
            const auto* heldWeapon = registry.try_get<EnemyHeldWeaponComponent>(entity);
            if(!heldWeapon || heldWeapon->weaponEntity == entt::null)
                return;    // 武器を持たない敵（ゾンビ系）

            if(!registry.IsValid(heldWeapon->weaponEntity) || !registry.HasComponent<WeaponComponent>(heldWeapon->weaponEntity))
                return;    // 既に破棄済み

            registry.GetComponent<WeaponComponent>(heldWeapon->weaponEntity).isAttacking = isAttacking;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief コンストラクタ。各ステートのOnEnterコールバック（クリップ切り替え）を登録する
    //-------------------------------------------------------------
    EnemyAnimationSystem::EnemyAnimationSystem() {
        // 待機用クリップが無いため、Idle/Walkともに歩行クリップを使う。
        // Idleはin_place=trueでその場足踏みにし、移動を表すWalkと視覚的に区別する必要はない
        // （追跡の有無自体はTransformの移動で表現される）
        m_stateMachine.RegisterState(EnemyAnimState::Idle, MakeClipEnterCallback(&EnemyAnimationSetComponent::walkClip, true, kAnimBlendTime, true));
        m_stateMachine.RegisterState(EnemyAnimState::Walk, MakeClipEnterCallback(&EnemyAnimationSetComponent::walkClip, true, kAnimBlendTime, true));

        // Attackへ入るときはattackTimer（ZombieBehavior::PlayAttackの終了判定ウォッチドッグ）も
        // ここでリセットする。PlayAttackはこのOnEnterが実行される1フレーム前から加算を始めているため、
        // 実際にAttackクリップへ切り替わった瞬間を基準に測り直す。
        // 併せてEnemyAttackHitboxComponentのhasLandedThisAttackもクリアし、
        // 新しい一振りで再度ヒットを取れるようにする
        // 併せて、手に武器を持っている敵（Paladin等）はここで武器を「攻撃中」にする。
        // Attackを抜けるときのOnExitで戻すことで、待機・歩行・のけぞり・死亡の間は
        // プレイヤーと同じ浮遊追従に戻る
        auto attackClipEnter = MakeClipEnterCallback(&EnemyAnimationSetComponent::attackClip, false, kAttackBlendTime, true);
        m_stateMachine.RegisterState(
            EnemyAnimState::Attack,
            [attackClipEnter](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
                attackClipEnter(registry, entity);
                registry.GetComponent<EnemyAnimationSetComponent>(entity).attackTimer = 0.0f;
                if(auto* hitbox = registry.try_get<EnemyAttackHitboxComponent>(entity)) {
                    hitbox->hasLandedThisAttack = false;
                    // 前フレームの判定点も破棄する。前の攻撃の終端位置から新しい攻撃の開始位置まで
                    // スイープしてしまうと、離れた場所にいるプレイヤーへ誤って当たることがあるため
                    hitbox->hasPrevSweepPoint = false;
                }
                SetHeldWeaponAttacking(registry, entity, true);
            },
            [](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
                SetHeldWeaponAttacking(registry, entity, false);
            });

        // Knockbackへ入るときはknockbackTimer（ZombieBehavior::PlayKnockbackの終了判定ウォッチドッグ）をリセットする
        auto knockbackClipEnter = MakeClipEnterCallback(&EnemyAnimationSetComponent::knockbackClip, false, kKnockbackBlendTime, true);
        m_stateMachine.RegisterState(EnemyAnimState::Knockback, [knockbackClipEnter](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
            knockbackClipEnter(registry, entity);
            registry.GetComponent<EnemyAnimationSetComponent>(entity).knockbackTimer = 0.0f;
        });

        // Deathへ入るときはdeathTimer（ZombieBehavior::PlayDeathの終了判定ウォッチドッグ）をリセットする
        auto deathClipEnter = MakeClipEnterCallback(&EnemyAnimationSetComponent::deathClip, false, kDeathBlendTime, true);
        m_stateMachine.RegisterState(EnemyAnimState::Death, [deathClipEnter](Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
            deathClipEnter(registry, entity);
            registry.GetComponent<EnemyAnimationSetComponent>(entity).deathTimer = 0.0f;
        });
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyAnimationSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        auto view = registry.View<EnemyAnimationSetComponent, Tsukino::BuiltIn::ECS::AnimationPlayerComponent>();
        view.each([&](entt::entity entity, EnemyAnimationSetComponent& animSet, Tsukino::BuiltIn::ECS::AnimationPlayerComponent&) {
            // EnemyBehaviorSystemが書いたdesiredStateへ委譲する。変化がなければ何もしない。
            // 変化していればOnExit→OnEnterの順でコールバックが呼ばれ、クリップが切り替わる
            m_stateMachine.TransitionTo(animSet.currentState, animSet.desiredState, registry, entity);
        });
    }
}    // namespace CombatAndroid::ECS
