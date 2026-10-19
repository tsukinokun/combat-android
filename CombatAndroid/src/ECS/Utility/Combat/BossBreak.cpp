//-------------------------------------------------------------
//! @file    BossBreak.cpp
//! @brief   ボス戦の駆け引き（ブレイク・ジャスト回避と反撃・後隙ボーナス）の共通処理の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Combat/BossBreak.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BossMoveSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Event/Player/PlayerFinisherEvent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    template <class Archive>
    void load(Archive& archive, BossCombatParams& params) {
        LoadField(archive, "justWindow", params.justWindow);
        LoadField(archive, "counterWindow", params.counterWindow);
        LoadField(archive, "counterDamageScale", params.counterDamageScale);
        LoadField(archive, "counterBreakScale", params.counterBreakScale);
        LoadField(archive, "recoveryDamageScale", params.recoveryDamageScale);
        LoadField(archive, "recoveryBreakScale", params.recoveryBreakScale);
        LoadField(archive, "downDamageScale", params.downDamageScale);
        LoadField(archive, "heavyBreakScale", params.heavyBreakScale);
        LoadField(archive, "regularSlowCooldown", params.regularSlowCooldown);
        LoadField(archive, "counterRimColor", params.counterRimColor);
        LoadField(archive, "counterRimIntensity", params.counterRimIntensity);
        LoadField(archive, "vulnerableRimColor", params.vulnerableRimColor);
        LoadField(archive, "vulnerableRimIntensity", params.vulnerableRimIntensity);
        LoadField(archive, "downRimIntensity", params.downRimIntensity);
        LoadField(archive, "vulnerableGlow", params.vulnerableGlow);
        LoadField(archive, "vulnerablePulseSpeed", params.vulnerablePulseSpeed);
    }

    //-------------------------------------------------------------
    //! @brief 駆け引きの調整値を得る
    //-------------------------------------------------------------
    const BossCombatParams& GetBossCombatParams() {
        static const BossCombatParams s_params = LoadSystemParams<BossCombatParams>("BossCombat");
        return s_params;
    }

    //-------------------------------------------------------------
    //! @brief ボスへの一撃にダウン・後隙・反撃の倍率を掛け、体勢ゲージを溜める
    //-------------------------------------------------------------
    float ApplyBossHitModifiers(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity attacker, Tsukino::ECS::Entity hitEntity, float damage,
                                float breakScale) {
        const BossCombatParams& params      = GetBossCombatParams();
        float                   damageScale = 1.0f;
        float                   gaugeScale  = breakScale;

        //-------------------------------------------------------------
        // 反撃の構え（ジャスト回避の直後）の一撃。雑魚にも効く。当たった時点で構えを解く
        //-------------------------------------------------------------
        if(auto* player = registry.try_get<PlayerComponent>(attacker); player && player->counterReadyTimer > 0.0f) {
            damageScale *= params.counterDamageScale;
            gaugeScale *= params.counterBreakScale;
            player->counterReadyTimer = 0.0f;
        }

        auto* boss = registry.try_get<BossMoveSetComponent>(hitEntity);
        if(!boss)
            return damage * damageScale;

        //-------------------------------------------------------------
        // ダウン中は大ダメージ。ゲージはダウンが明けるまで溜めない（明けた直後にまたダウンさせないため）
        //-------------------------------------------------------------
        const bool broken = boss->phase == BossActionPhase::Broken;
        if(broken) {
            damageScale *= params.downDamageScale;
        } else if(boss->phase == BossActionPhase::Recovery) {
            damageScale *= params.recoveryDamageScale;
            gaugeScale *= params.recoveryBreakScale;
        }

        if(!broken && boss->breakGaugeMax > 0.0f) {
            boss->breakGauge     = std::min(boss->breakGauge + damage * gaugeScale, boss->breakGaugeMax);
            boss->breakIdleTimer = 0.0f;
            if(boss->breakGauge >= boss->breakGaugeMax)
                boss->breakPending = true;
        }

        return damage * damageScale;
    }

    //-------------------------------------------------------------
    //! @brief 回避の無敵でボスの攻撃をすり抜けた瞬間に、ジャスト回避かを判定する
    //-------------------------------------------------------------
    bool TryJustDodge(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity playerEntity, PlayerComponent& player, Tsukino::ECS::Entity attacker) {
        if(!player.isDodging || player.counterReadyTimer > 0.0f)
            return false;
        if(attacker == entt::null || !registry.IsValid(attacker) || !registry.HasComponent<EnemyComponent>(attacker))
            return false;

        const auto* animSet = registry.try_get<PlayerAnimationSetComponent>(playerEntity);
        if(!animSet)
            return false;

        const BossCombatParams& params = GetBossCombatParams();
        if(animSet->dodgeTimer > params.justWindow)
            return false;

        player.counterReadyTimer = params.counterWindow;

        //-------------------------------------------------------------
        // 大技と同じ「スロー＋カメラの寄り」を、待ち無しで掛ける。
        // ボスは毎回。雑魚は群れに囲まれるたびに掛からないよう、一度掛けたらしばらく掛けない
        //-------------------------------------------------------------
        const bool isBoss = registry.HasComponent<BossMoveSetComponent>(attacker);
        if(isBoss || player.justDodgeSlowCooldown <= 0.0f) {
            if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                eventBus->Publish(PlayerFinisherEvent{playerEntity, 0.0f});
            if(!isBoss)
                player.justDodgeSlowCooldown = params.regularSlowCooldown;
        }

#ifdef _DEBUG
        Tsukino::Core::Log::Info("JustDodge dodgeTimer=" + std::to_string(animSet->dodgeTimer));
#endif
        return true;
    }
}    // namespace CombatAndroid::ECS
