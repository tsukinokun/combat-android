//-------------------------------------------------------------
//! @file    WeaponEvolutionTable.cpp
//! @brief   武器の進化の表の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/WeaponEvolutionTable.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Event/GameLogEvent.hpp>

#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // WeaponEvolutionEntryのcerealシリアライズ定義（条件のスキルはスキル名で持つ）
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const WeaponEvolutionEntry& entry) {
        archive(cereal::make_nvp("requiredSkill", std::string(GetSkillKey(entry.requiredSkill))),
                cereal::make_nvp("requiredSkillLevel", entry.requiredSkillLevel));
        SaveWideField(archive, "displayName", entry.displayName);
        archive(cereal::make_nvp("damageScale", entry.damageScale),
                cereal::make_nvp("areaRadiusScale", entry.areaRadiusScale),
                cereal::make_nvp("areaKnockbackSpeed", entry.areaKnockbackSpeed),
                cereal::make_nvp("areaKnockbackStun", entry.areaKnockbackStun),
                cereal::make_nvp("rangeScale", entry.rangeScale),
                cereal::make_nvp("hitCapsuleRadiusScale", entry.hitCapsuleRadiusScale),
                cereal::make_nvp("projectilePierceMinChargeStage", entry.projectilePierceMinChargeStage),
                cereal::make_nvp("projectileRadiusScale", entry.projectileRadiusScale),
                cereal::make_nvp("projectileDistanceScale", entry.projectileDistanceScale));
    }

    template <class Archive>
    void load(Archive& archive, WeaponEvolutionEntry& entry) {
        std::string requiredSkill = GetSkillKey(entry.requiredSkill);
        LoadField(archive, "requiredSkill", requiredSkill);
        if(!FindSkillByKey(requiredSkill, entry.requiredSkill))
            Tsukino::Core::Log::Error("WeaponEvolution.json: unknown skill " + requiredSkill);

        LoadField(archive, "requiredSkillLevel", entry.requiredSkillLevel);
        LoadWideField(archive, "displayName", entry.displayName);
        LoadField(archive, "damageScale", entry.damageScale);
        LoadField(archive, "areaRadiusScale", entry.areaRadiusScale);
        LoadField(archive, "areaKnockbackSpeed", entry.areaKnockbackSpeed);
        LoadField(archive, "areaKnockbackStun", entry.areaKnockbackStun);
        LoadField(archive, "rangeScale", entry.rangeScale);
        LoadField(archive, "hitCapsuleRadiusScale", entry.hitCapsuleRadiusScale);
        LoadField(archive, "projectilePierceMinChargeStage", entry.projectilePierceMinChargeStage);
        LoadField(archive, "projectileRadiusScale", entry.projectileRadiusScale);
        LoadField(archive, "projectileDistanceScale", entry.projectileDistanceScale);
    }

    namespace {
        constexpr const char* kEvolutionFile = "WeaponEvolution.json";
        constexpr const char* kEvolutionRoot = "WeaponEvolution";

        //-------------------------------------------------------------
        //! @struct WeaponEvolutionData
        //! @brief  進化の表全体（WeaponIdの並び順。JSONのキーは武器名）
        //-------------------------------------------------------------
        struct WeaponEvolutionData {
            std::array<WeaponEvolutionEntry, static_cast<size_t>(WeaponId::Count)> entries{};
        };

        template <class Archive>
        void save(Archive& archive, const WeaponEvolutionData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i)
                archive(cereal::make_nvp(GetWeaponKey(static_cast<WeaponId>(i)), data.entries[i]));
        }

        template <class Archive>
        void load(Archive& archive, WeaponEvolutionData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i)
                LoadField(archive, GetWeaponKey(static_cast<WeaponId>(i)), data.entries[i]);
        }

        //-------------------------------------------------------------
        //! @brief  進化の表を Assets/Tables/WeaponEvolution.json から読む関数
        //! @return WeaponIdの並び順の表（JSONに無い武器は進化しない）
        //-------------------------------------------------------------
        WeaponEvolutionData LoadWeaponEvolution() {
            WeaponEvolutionData data;
            for(size_t i = 0; i < data.entries.size(); ++i)
                data.entries[i].weaponId = static_cast<WeaponId>(i);

            (void)LoadTableJson(kEvolutionFile, kEvolutionRoot, data);
            return data;
        }

        //! 進化した瞬間に武器へ焼く発光の長さ（秒）。PickupSystemはレベルアップ発光の長さ（0.45秒）で
        //! 割った値を0〜1に丸めて明るさにするので、それより長く入れると最大の明るさがしばらく続いてから消える
        constexpr float kEvolveFlashDuration = 1.2f;

        //-------------------------------------------------------------
        //! @brief  進化の表を得る関数（初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全）
        //-------------------------------------------------------------
        const WeaponEvolutionData& GetWeaponEvolutionData() {
            static const WeaponEvolutionData s_data = LoadWeaponEvolution();
            return s_data;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 武器の種類から進化の設定を引く
    //-------------------------------------------------------------
    const WeaponEvolutionEntry& GetWeaponEvolution(WeaponId id) {
        const int index = std::clamp(static_cast<int>(id), 0, static_cast<int>(WeaponId::Count) - 1);
        return GetWeaponEvolutionData().entries[static_cast<size_t>(index)];
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
