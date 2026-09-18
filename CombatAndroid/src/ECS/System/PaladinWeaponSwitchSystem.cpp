//-------------------------------------------------------------
//! @file   PaladinWeaponSwitchSystem.cpp
//! @brief  PaladinWeaponSwitchSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PaladinWeaponSwitchSystem.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyHeldWeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PaladinArsenalComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Utility/EliteEnemy.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <entt/entt.hpp>

#include <climits>
#include <cstdlib>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 使い分けの距離（水平距離・ユニット）。
        // エリートの間合いはグレートソード195・バトルアックス182・ウォーハンマー169なので、
        // 振り終わりにそれより大きく離れていれば「逃げられた」、判定の太さ程度まで
        // 詰められていれば「懐に入られた」と見なす
        //-------------------------------------------------------------
        constexpr float kFarDistance   = 260.0f;    //!< これより遠ければグレートソード
        constexpr float kCloseDistance = 130.0f;    //!< これより近ければウォーハンマー

        //! 同じ武器をこの回数続けて選んだら、次は別の武器にする
        constexpr int kMaxSameWeaponStreak = 2;

        //-------------------------------------------------------------
        //! @brief  武器の「間合いの長さ」の順位（0が一番遠くから届く）
        //! @param  id [in] 武器の種類
        //-------------------------------------------------------------
        [[nodiscard]]
        int GetReachRank(WeaponId id) {
            switch(id) {
            case WeaponId::Greatsword: return 0;
            case WeaponId::Battleaxe:  return 1;
            default:                   return 2;    // Warhammer
            }
        }

        //-------------------------------------------------------------
        //! @brief  プレイヤーとの距離から、使いたい武器の間合いの順位を決める関数
        //! @param  distance [in] プレイヤーとの水平距離
        //! @return 0＝遠い（グレートソード）、1＝中間（バトルアックス）、2＝近い（ウォーハンマー）
        //-------------------------------------------------------------
        [[nodiscard]]
        int ChoosePreferredReachRank(float distance) {
            if(distance > kFarDistance)
                return 0;
            if(distance < kCloseDistance)
                return 2;
            return 1;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PaladinWeaponSwitchSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        auto* context = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!context)
            return;

        //-------------------------------------------------------------
        // プレイヤーの位置（単一プレイヤー前提）
        //-------------------------------------------------------------
        bool           hasPlayer      = false;
        hlslpp::float3 playerPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);
        auto           playerView     = registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(auto entity : playerView) {
            playerPosition = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
            hasPlayer      = true;
            break;
        }
        if(!hasPlayer)
            return;

        //-------------------------------------------------------------
        // 攻撃ステートを抜けた個体を集める。持ち替えは他のコンポーネントを書き換えるので、
        // Viewの外でまとめて行う
        //-------------------------------------------------------------
        struct SwitchRequest {
            Tsukino::ECS::Entity enemy;
            float                distance;
        };
        std::vector<SwitchRequest> requests;

        auto view = registry.View<PaladinArsenalComponent, EnemyAnimationSetComponent, HealthComponent,
                                  Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](Tsukino::ECS::Entity entity, PaladinArsenalComponent& arsenal, const EnemyAnimationSetComponent& animSet,
                      const HealthComponent& health, const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            const bool leftAttack = arsenal.previousState == EnemyAnimState::Attack && animSet.currentState != EnemyAnimState::Attack;
            arsenal.previousState = animSet.currentState;

            // 死んだ個体は持ち替えない（持っている武器はそのまま落とす）
            if(!leftAttack || health.isDead || animSet.currentState == EnemyAnimState::Death)
                return;

            hlslpp::float3 toPlayer = playerPosition - transform.position;
            toPlayer.y              = 0.0f;
            requests.push_back({entity, hlslpp::length(toPlayer)});
        });

        for(const SwitchRequest& request : requests) {
            auto& arsenal = registry.GetComponent<PaladinArsenalComponent>(request.enemy);
            auto& held    = registry.GetComponent<EnemyHeldWeaponComponent>(request.enemy);

            //-------------------------------------------------------------
            // 距離で使いたい間合いを決め、持っている武器のうちそれに一番近いものを選ぶ。
            // 個体によって持っている種類が違うので、ちょうどの武器が無ければ
            // 隣の間合いの武器で代える（同じだけ近いものが2本あればどちらかを無作為に）。
            // 同じ武器が続きすぎるときは、今の武器以外から無作為に選んで替える
            //-------------------------------------------------------------
            const int             preferredRank = ChoosePreferredReachRank(request.distance);
            std::vector<WeaponId> candidates;
            int                   bestGap = INT_MAX;
            for(Tsukino::ECS::Entity weaponEntity : arsenal.weaponEntities) {
                const auto* weapon = registry.try_get<WeaponComponent>(weaponEntity);
                if(!weapon)
                    continue;

                const int gap = std::abs(GetReachRank(weapon->weaponId) - preferredRank);
                if(gap < bestGap) {
                    bestGap = gap;
                    candidates.clear();
                }
                if(gap == bestGap)
                    candidates.push_back(weapon->weaponId);
            }
            if(candidates.empty())
                continue;

            std::uniform_int_distribution<size_t> candidateDist(0, candidates.size() - 1);
            WeaponId nextId = candidates[candidateDist(m_rng)];

            if(nextId == held.weaponId && arsenal.sameWeaponStreak >= kMaxSameWeaponStreak) {
                std::vector<WeaponId> others;
                for(Tsukino::ECS::Entity weaponEntity : arsenal.weaponEntities) {
                    if(const auto* weapon = registry.try_get<WeaponComponent>(weaponEntity); weapon && weapon->weaponId != held.weaponId)
                        others.push_back(weapon->weaponId);
                }
                if(!others.empty()) {
                    std::uniform_int_distribution<size_t> dist(0, others.size() - 1);
                    nextId = others[dist(m_rng)];
                }
            }

            arsenal.sameWeaponStreak = (nextId == held.weaponId) ? arsenal.sameWeaponStreak + 1 : 1;
            if(nextId == held.weaponId)
                continue;

            for(Tsukino::ECS::Entity weaponEntity : arsenal.weaponEntities) {
                const auto* weapon = registry.try_get<WeaponComponent>(weaponEntity);
                if(weapon && weapon->weaponId == nextId) {
                    SwitchPaladinWeapon(registry, *context, request.enemy, weaponEntity);
                    break;
                }
            }
        }
    }
}    // namespace CombatAndroid::ECS
