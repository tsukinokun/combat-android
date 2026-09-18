//-------------------------------------------------------------
//! @file   EnemyAttackTelegraphSystem.cpp
//! @brief  EnemyAttackTelegraphSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/EnemyAttackTelegraphSystem.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 予兆の見た目のチューニング値。
        // 敵は数が多いので、常時光る演出ではなく「振りかぶりの間だけ赤くなる」ことを狙う
        //-------------------------------------------------------------
        const hlslpp::float3 kTelegraphColor = hlslpp::float3(1.0f, 0.12f, 0.08f);    //!< 赤。スキル・拾得の金色と混ざらない色味にする

        constexpr float kRimIntensityMax = 6.0f;     //!< 判定が出る直前のふちの強さ
        constexpr float kGlowMax         = 0.30f;    //!< 同じく面全体の持ち上げ量（強くすると白飛びする）
        constexpr float kRimPower        = 2.2f;     //!< ふちの鋭さ。溜め攻撃（2.5）よりわずかに広く出す

        //! 振りかぶりの何割を過ぎてから光らせ始めるか。
        //! 0から光らせると「攻撃に入った瞬間」に全員が赤くなり、かえって読み取りにくい
        constexpr float kStartRatio = 0.25f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyAttackTelegraphSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {

        auto view = registry.View<EnemyComponent, EnemyAnimationSetComponent, EnemyAttackHitboxComponent, HealthComponent,
                                  Tsukino::BuiltIn::ECS::RimGlowComponent>();

        view.each([](EnemyComponent&, const EnemyAnimationSetComponent& animSet, const EnemyAttackHitboxComponent& hitbox,
                     const HealthComponent& health, Tsukino::BuiltIn::ECS::RimGlowComponent& highlight) {
            //-------------------------------------------------------------
            // 予兆を出す条件：攻撃モーション中で、まだ判定が出ていないこと。
            // 死亡中は死亡演出（フェード）の邪魔になるので出さない
            //-------------------------------------------------------------
            const bool isWindingUp = !health.isDead && animSet.currentState == EnemyAnimState::Attack
                                     && animSet.attackTimer < hitbox.hitStartTime && hitbox.hitStartTime > 0.0f;

            if(!isWindingUp) {
                // 消灯。他に敵のRimGlowを書くSystemは無いので、ここで落とし切ってよい
                highlight.active       = false;
                highlight.rimIntensity = 0.0f;
                highlight.glow         = 0.0f;
                return;
            }

            // 判定が出る瞬間へ向けて強くする。kStartRatioまでは消灯のまま溜める
            const float windupProgress = std::clamp(animSet.attackTimer / hitbox.hitStartTime, 0.0f, 1.0f);
            const float strength       = std::clamp((windupProgress - kStartRatio) / (1.0f - kStartRatio), 0.0f, 1.0f);
            if(strength <= 0.0f) {
                highlight.active = false;
                return;
            }

            highlight.active       = true;
            highlight.rimColor     = kTelegraphColor;
            highlight.rimIntensity = kRimIntensityMax * strength;
            highlight.rimPower     = kRimPower;
            highlight.glow         = kGlowMax * strength;
        });
    }
}    // namespace CombatAndroid::ECS
