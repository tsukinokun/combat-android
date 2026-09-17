//-------------------------------------------------------------
//! @file    GameplayFreeze.cpp
//! @brief   ゲームの進行を止める（メニュー表示中）判定と、止めている間の後始末の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/GameplayFreeze.hpp>

#include <CombatAndroid/ECS/Component/HitStopComponent.hpp>
#include <CombatAndroid/ECS/System/PauseMenuSystem.hpp>
#include <CombatAndroid/ECS/System/RunResultSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>

#include <entt/entt.hpp>

#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief 今ゲームの進行を止めているかを問い合わせる
    //-------------------------------------------------------------
    bool IsGameplayFrozen(Tsukino::ECS::Registry& registry) {
        return IsSkillSelectActive(registry) || IsPauseMenuActive(registry) || IsRunResultFreezing(registry);
    }

    //-------------------------------------------------------------
    //! @brief 全キャラクタの移動入力を打ち消す
    //-------------------------------------------------------------
    void SuppressAllMoveInput(Tsukino::ECS::Registry& registry) {
        auto view = registry.View<Tsukino::BuiltIn::ECS::CharacterControllerComponent>();
        view.each([](Tsukino::BuiltIn::ECS::CharacterControllerComponent& characterController) {
            characterController.moveInput = hlslpp::float3(0.0f, 0.0f, 0.0f);
        });
    }

    //-------------------------------------------------------------
    //! @brief 進行中のヒットストップを全エンティティから取り除く
    //-------------------------------------------------------------
    void ClearAllHitStop(Tsukino::ECS::Registry& registry) {
        std::vector<entt::entity> entities;
        auto                      view = registry.View<HitStopComponent>();
        for(entt::entity entity : view)
            entities.push_back(entity);

        for(entt::entity entity : entities) {
            // HitStopSystemの自然終了パスと同様、削除前にplayback_speedを
            // ヒットストップ開始時点の値へ復元する。これをしないと停止とヒットストップが
            // 重なった際にアニメーション速度が固まったまま戻らなくなる
            const HitStopComponent& hitStop = view.get<HitStopComponent>(entity);
            if(auto* animPlayer = registry.try_get<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity)) {
                animPlayer->playback_speed = hitStop.baseAnimSpeed;
            }
            registry.RemoveComponent<HitStopComponent>(entity);
        }
    }
}    // namespace CombatAndroid::ECS
