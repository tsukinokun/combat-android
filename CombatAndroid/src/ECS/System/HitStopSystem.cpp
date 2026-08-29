//-------------------------------------------------------------
//! @file   HitStopSystem.cpp
//! @brief  HitStopSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/HitStopSystem.hpp>
#include <CombatAndroid/ECS/Component/HitStopComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>

#include <entt/entt.hpp>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void HitStopSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        std::vector<entt::entity> expired;

        auto view = registry.View<HitStopComponent>();
        view.each([&](entt::entity entity, HitStopComponent& hitStop) {
            // remainingTimeは実時間で減算する（シーン全体のdeltaTimeはもうヒットストップで
            // 縮小されていないため、そのまま使ってよい）
            hitStop.remainingTime -= deltaTime;
            if(hitStop.remainingTime <= 0.0f) {
                // 終了：playback_speedをヒットストップ開始時点の値へ明示的に復元する。
                // playback_speedはコンボ段/回避の切り替わり時にしか書き換わらないため、
                // ここで戻さないとエンティティによっては次の切り替わりまで
                // 遅いままになってしまう（＝アニメーションが固まって見える不具合の原因だった）
                if(auto* animPlayer = registry.try_get<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity)) {
                    animPlayer->playback_speed = hitStop.baseAnimSpeed;
                }
                expired.push_back(entity);
                return;
            }

            // アニメーション減速：baseAnimSpeed（ヒットストップ開始時点の速度）へ毎フレーム
            // 掛け直す。現在値へ掛け算すると複数フレームぶん乗算されて0近くまで潰れてしまうため、
            // 必ずこの基準値からの掛け算にする
            if(auto* animPlayer = registry.try_get<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(entity)) {
                animPlayer->playback_speed = hitStop.baseAnimSpeed * hitStop.scale;
            }

            // 移動停止：PhysicsSystemはdeltaTimeが0以下でも1/60秒ぶんステップしてしまうため、
            // moveInputそのものを潰しておかないと滑り続ける（SkillSelectSystemの
            // SuppressMoveInputと同じ理由・同じ対処。対象をこのエンティティだけに絞っている点が異なる）
            if(auto* controller = registry.try_get<Tsukino::BuiltIn::ECS::CharacterControllerComponent>(entity)) {
                controller->moveInput = hlslpp::float3(0.0f, 0.0f, 0.0f);
            }
        });

        for(entt::entity entity : expired) {
            registry.RemoveComponent<HitStopComponent>(entity);
        }
    }

}    // namespace CombatAndroid::ECS
