//-------------------------------------------------------------
//! @file   SoftTarget.cpp
//! @brief  ソフトターゲットと攻撃時の振り向きの共通処理の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Combat/SoftTarget.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/SoftTargetComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <cmath>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void load(Archive& archive, SoftTargetParams& params) {
        LoadField(archive, "selectRange", params.selectRange);
        LoadField(archive, "assistRange", params.assistRange);
        LoadField(archive, "assistAngleDeg", params.assistAngleDeg);
        LoadField(archive, "angleWeight", params.angleWeight);
        LoadField(archive, "distanceWeight", params.distanceWeight);
        LoadField(archive, "stickiness", params.stickiness);
        LoadField(archive, "switchCooldown", params.switchCooldown);
        LoadField(archive, "turnDuration", params.turnDuration);
    }

    //-------------------------------------------------------------
    //! @brief  調整値を得る
    //-------------------------------------------------------------
    const SoftTargetParams& GetSoftTargetParams() {
        return GetSystemParams<SoftTargetParams>("SoftTarget");
    }

    //-------------------------------------------------------------
    //! @brief  基準の向きを求める
    //-------------------------------------------------------------
    hlslpp::float3 ResolveAimDirection(const PlayerComponent& player, const hlslpp::quaternion& rotation) {
        const hlslpp::float3 input(player.aimInputDirection.x, 0.0f, player.aimInputDirection.z);
        const float          inputLength = hlslpp::length(input);
        if(inputLength > 0.001f)
            return input / inputLength;

        // 入力が無ければキャラの正面。PlayerSystemはrotation_y(atan2(x, z))で向きを作るので、ローカル前方は+Z
        hlslpp::float3 forward = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), rotation);
        forward.y              = 0.0f;
        const float length     = hlslpp::length(forward);
        return length > 0.001f ? forward / length : hlslpp::float3(0.0f, 0.0f, 1.0f);
    }

    //-------------------------------------------------------------
    //! @brief  攻撃時の振り向きを要求する
    //-------------------------------------------------------------
    void RequestAttackTurnAssist(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity player) {
        auto* softTarget = registry.try_get<SoftTargetComponent>(player);
        auto* transform  = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(player);
        if(!softTarget || !transform)
            return;

        if(!softTarget->assistReady || softTarget->target == entt::null || !registry.IsValid(softTarget->target))
            return;

        // 選ばれた後に倒れた敵へは振り向かない（同じフレームの中で死んだ場合）
        if(const auto* health = registry.try_get<HealthComponent>(softTarget->target); health && health->isDead)
            return;

        const auto* targetTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(softTarget->target);
        if(!targetTransform)
            return;

        const hlslpp::float3 toTarget(targetTransform->position.x - transform->position.x, 0.0f,
                                      targetTransform->position.z - transform->position.z);
        if(static_cast<float>(hlslpp::length(toTarget)) < 0.001f)
            return;

        //-------------------------------------------------------------
        // 向きだけを変える（Yaw）。位置は動かさない
        //-------------------------------------------------------------
        const float yaw         = std::atan2(static_cast<float>(toTarget.x), static_cast<float>(toTarget.z));
        softTarget->turnFrom    = transform->rotation;
        softTarget->turnTo      = hlslpp::quaternion::rotation_y(yaw);
        softTarget->turnElapsed = 0.0f;
        softTarget->turning     = true;
    }
}    // namespace CombatAndroid::ECS
