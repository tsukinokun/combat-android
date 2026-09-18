//-------------------------------------------------------------
//! @file    WeaponEvolutionTable.cpp
//! @brief   武器の進化の表の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/WeaponEvolutionTable.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Event/GameLogEvent.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 進化の表本体。
        //
        // ★ 進化の条件や効果を調整したいときはここの数値だけを触ればよい ★
        //
        // 組み合わせは武器の持ち味を伸ばす方向で選んでいる。
        //   ウォーハンマー × 憤怒：3段目の衝撃波を広げ、巻き込んだ敵を吹き飛ばす
        //   グレートソード × 傲慢：刃を長く太くして、間合いの外から斬れるようにする
        //   バトルアックス × 嫉妬：斬撃弾を溜め1段目から貫通させ、太く遠くまで飛ばす
        // GetWeaponEvolutionがidを添字として使うため、必ずWeaponIdの並び順に定義すること
        //-------------------------------------------------------------
        constexpr WeaponEvolutionEntry kWeaponEvolutionTable[] = {
            //  武器                 条件スキル       Lv  進化後の名前      ダメ   範囲   吹飛初速 吹飛スタン 射程  太さ   貫通 弾半径 弾距離
            {WeaponId::Warhammer,  SkillId::Wrath, 3, L"憤怒の鎚",   1.3f, 1.5f, 700.0f, 0.30f, 1.0f, 1.0f, 0, 1.0f, 1.0f},
            {WeaponId::Greatsword, SkillId::Pride, 3, L"傲慢の大剣", 1.3f, 1.45f, 0.0f,  0.0f, 1.45f, 1.45f, 0, 1.0f, 1.0f},
            {WeaponId::Battleaxe,  SkillId::Envy,  3, L"嫉妬の斧",   1.3f, 1.0f, 0.0f,   0.0f, 1.0f, 1.0f, 1, 1.43f, 1.5f},
        };

        // 武器を足したのに表へ書き忘れる事故を防ぐ
        static_assert(std::size(kWeaponEvolutionTable) == static_cast<size_t>(WeaponId::Count),
                      "WeaponId に種類を足したら kWeaponEvolutionTable にも1行足すこと");

        //! 進化した瞬間に武器へ焼く発光の長さ（秒）。PickupSystemはレベルアップ発光の長さ（0.45秒）で
        //! 割った値を0〜1に丸めて明るさにするので、それより長く入れると最大の明るさがしばらく続いてから消える
        constexpr float kEvolveFlashDuration = 1.2f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 武器の種類から進化の設定を引く
    //-------------------------------------------------------------
    const WeaponEvolutionEntry& GetWeaponEvolution(WeaponId id) {
        int index = std::clamp(static_cast<int>(id), 0, static_cast<int>(WeaponId::Count) - 1);
        return kWeaponEvolutionTable[index];
    }

    //-------------------------------------------------------------
    //! @brief 武器を進化させる
    //-------------------------------------------------------------
    void ApplyWeaponEvolution(WeaponComponent& weapon) {
        if(weapon.evolved)
            return;

        const WeaponEvolutionEntry& entry = GetWeaponEvolution(weapon.weaponId);

        weapon.evolved = true;
        RecalculateWeaponStats(weapon);    // evolvedを見てdamageScaleを掛ける

        weapon.areaAttackRadius *= entry.areaRadiusScale;
        weapon.areaKnockbackSpeed = std::max(weapon.areaKnockbackSpeed, entry.areaKnockbackSpeed);
        weapon.areaKnockbackStun  = std::max(weapon.areaKnockbackStun, entry.areaKnockbackStun);

        weapon.range *= entry.rangeScale;
        weapon.hitCapsuleRadius *= entry.hitCapsuleRadiusScale;

        if(entry.projectilePierceMinChargeStage > 0)
            weapon.projectilePierceMinChargeStage = entry.projectilePierceMinChargeStage;
        weapon.projectileRadius *= entry.projectileRadiusScale;
        weapon.projectileMaxDistance *= entry.projectileDistanceScale;
        weapon.projectileLifetime *= entry.projectileDistanceScale;    // 寿命で先に消えないよう距離と揃えて伸ばす
    }

    //-------------------------------------------------------------
    //! @brief プレイヤーの持つ武器のうち、条件を満たしたものを進化させる
    //-------------------------------------------------------------
    void TryEvolvePlayerWeapons(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity playerEntity) {
        if(!registry.IsValid(playerEntity))
            return;

        auto* player = registry.try_get<PlayerComponent>(playerEntity);
        auto* skills = registry.try_get<PlayerSkillComponent>(playerEntity);
        if(!player || !skills)
            return;

        for(Tsukino::ECS::Entity weaponEntity : player->weaponInventory) {
            auto* weapon = registry.try_get<WeaponComponent>(weaponEntity);
            if(!weapon || weapon->evolved || weapon->level < kMaxWeaponLevel)
                continue;

            const WeaponEvolutionEntry& entry = GetWeaponEvolution(weapon->weaponId);
            if(skills->levels[static_cast<size_t>(entry.requiredSkill)] < entry.requiredSkillLevel)
                continue;

            ApplyWeaponEvolution(*weapon);
            weapon->levelUpFlashTimer = kEvolveFlashDuration;

            // 取得ログへ流す（効果音もGameSoundSystemがこの種別から鳴らす）
            if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                eventBus->Publish(GameLogEvent{GameLogCategory::WeaponEvolved, entry.displayName});
        }
    }
}    // namespace CombatAndroid::ECS
