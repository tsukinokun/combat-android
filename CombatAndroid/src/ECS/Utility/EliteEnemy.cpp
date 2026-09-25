//-------------------------------------------------------------
//! @file    EliteEnemy.cpp
//! @brief   エリート敵の抽選と強化の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/EliteEnemy.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyHeldWeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/PaladinArsenalComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // EliteSettingsのcerealシリアライズ定義。呼び名は "displayNames": { 敵の名前: 呼び名 } で持つ
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const EliteSettings& settings) {
        archive(cereal::make_nvp("maxLiveElites", settings.maxLiveElites),
                cereal::make_nvp("firstEliteRank", settings.firstEliteRank),
                cereal::make_nvp("chanceAtFirstRank", settings.chanceAtFirstRank),
                cereal::make_nvp("chancePerRank", settings.chancePerRank),
                cereal::make_nvp("chanceMax", settings.chanceMax),
                cereal::make_nvp("finalStretchChanceScale", settings.finalStretchChanceScale),
                cereal::make_nvp("sizeScale", settings.sizeScale),
                cereal::make_nvp("healthScale", settings.healthScale),
                cereal::make_nvp("damageScale", settings.damageScale),
                cereal::make_nvp("thresholdScale", settings.thresholdScale),
                cereal::make_nvp("moveSpeedScale", settings.moveSpeedScale),
                cereal::make_nvp("expRewardScale", settings.expRewardScale),
                cereal::make_nvp("knockbackDecayScale", settings.knockbackDecayScale),
                cereal::make_nvp("glowColor", settings.glowColor),
                cereal::make_nvp("arsenalSpacing", settings.arsenalSpacing),
                cereal::make_nvp("arsenalHeight", settings.arsenalHeight),
                cereal::make_nvp("arsenalDepth", settings.arsenalDepth));

        archive.setNextName("displayNames");
        archive.startNode();
        for(size_t i = 0; i < settings.displayNames.size(); ++i)
            SaveWideField(archive, GetEnemyTypeKey(static_cast<EnemyTypeId>(i)), settings.displayNames[i]);
        archive.finishNode();
    }

    template <class Archive>
    void load(Archive& archive, EliteSettings& settings) {
        LoadField(archive, "maxLiveElites", settings.maxLiveElites);
        LoadField(archive, "firstEliteRank", settings.firstEliteRank);
        LoadField(archive, "chanceAtFirstRank", settings.chanceAtFirstRank);
        LoadField(archive, "chancePerRank", settings.chancePerRank);
        LoadField(archive, "chanceMax", settings.chanceMax);
        LoadField(archive, "finalStretchChanceScale", settings.finalStretchChanceScale);
        LoadField(archive, "sizeScale", settings.sizeScale);
        LoadField(archive, "healthScale", settings.healthScale);
        LoadField(archive, "damageScale", settings.damageScale);
        LoadField(archive, "thresholdScale", settings.thresholdScale);
        LoadField(archive, "moveSpeedScale", settings.moveSpeedScale);
        LoadField(archive, "expRewardScale", settings.expRewardScale);
        LoadField(archive, "knockbackDecayScale", settings.knockbackDecayScale);
        LoadField(archive, "glowColor", settings.glowColor);
        LoadField(archive, "arsenalSpacing", settings.arsenalSpacing);
        LoadField(archive, "arsenalHeight", settings.arsenalHeight);
        LoadField(archive, "arsenalDepth", settings.arsenalDepth);

        // 呼び名は入れ子のオブジェクト。キーが無ければ既定値（"敵"）のまま
        try {
            archive.setNextName("displayNames");
            archive.startNode();
        } catch(const cereal::Exception&) {
            return;
        }
        for(size_t i = 0; i < settings.displayNames.size(); ++i)
            LoadWideField(archive, GetEnemyTypeKey(static_cast<EnemyTypeId>(i)), settings.displayNames[i]);
        archive.finishNode();
    }

    namespace {
        constexpr const char* kEliteFile = "Elite.json";
        constexpr const char* kEliteRoot = "Elite";

        //! 呼び名がJSONに無い敵に使う名前
        const std::wstring kFallbackDisplayName = L"敵";

        //-------------------------------------------------------------
        //! @brief  エリートの設定を Assets/Tables/Elite.json から読む関数
        //! @return 読んだ設定（読めなかった項目は既定値＝強化なし）
        //-------------------------------------------------------------
        EliteSettings LoadEliteSettings() {
            EliteSettings settings;
            settings.displayNames.fill(kFallbackDisplayName);
            (void)LoadTableJson(kEliteFile, kEliteRoot, settings);

            // 閾値だけ伸ばすと一撃で怯まない硬さが体力以上に跳ね上がる（EnemyDifficultyTableと同じ約束）
            if(settings.thresholdScale > settings.healthScale)
                Tsukino::Core::Log::Error("Elite.json: thresholdScale must not exceed healthScale");
            return settings;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief エリートの設定を得る（初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全）
    //-------------------------------------------------------------
    const EliteSettings& GetEliteSettings() {
        static const EliteSettings s_settings = LoadEliteSettings();
        return s_settings;
    }

    //-------------------------------------------------------------
    //! @brief 今回湧かせる1体をエリートにするかを抽選する
    //-------------------------------------------------------------
    bool RollElite(std::mt19937& rng, int dangerRank, bool isFinalStretch, int liveEliteCount) {
        const EliteSettings& settings = GetEliteSettings();
        if(dangerRank < settings.firstEliteRank || liveEliteCount >= settings.maxLiveElites)
            return false;

        float chance = std::min(settings.chanceAtFirstRank + settings.chancePerRank * static_cast<float>(dangerRank - settings.firstEliteRank),
                                settings.chanceMax);
        if(isFinalStretch)
            chance *= settings.finalStretchChanceScale;

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng) < chance;
    }

    //-------------------------------------------------------------
    //! @brief 生成設定をエリート用に強化する
    //! @note  見た目の大きさを変えるなら、体の当たり・攻撃の届く距離・攻撃判定の太さも
    //!        同じだけ変えないと「大きいのに当たらない」「HPバーが体に埋まる」になる
    //!        （sizeScaleはEnemySpawner.cppのApplyScalesがそれらへまとめて掛ける）
    //-------------------------------------------------------------
    void ApplyEliteModifiers(EnemySpawnConfig& config) {
        const EliteSettings& settings = GetEliteSettings();

        config.sizeScale *= settings.sizeScale;

        config.healthScale *= settings.healthScale;
        config.attackScale *= settings.damageScale;
        config.knockbackThresholdScale *= settings.thresholdScale;
        config.moveSpeedScale *= settings.moveSpeedScale;
        config.expScale *= settings.expRewardScale;
    }

    //-------------------------------------------------------------
    //! @brief 出現ログに出す敵の呼び名
    //-------------------------------------------------------------
    const std::wstring& GetEliteDisplayName(EnemyTypeId id) {
        const int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(EnemyTypeId::Count))
            return kFallbackDisplayName;
        return GetEliteSettings().displayNames[static_cast<size_t>(index)];
    }

    //-------------------------------------------------------------
    //! @brief エリートのPaladinに、手持ちとは別の種類の武器を足して2本以上持たせる
    //-------------------------------------------------------------
    void EquipPaladinArsenal(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                             std::mt19937& rng, Tsukino::ECS::Entity enemyEntity) {
        const auto* held = registry.try_get<EnemyHeldWeaponComponent>(enemyEntity);
        if(!held || held->weaponEntity == entt::null || !context.assetManager)
            return;

        const Tsukino::ECS::Entity heldWeaponEntity = held->weaponEntity;
        const WeaponId             heldWeaponId     = held->weaponId;
        const hlslpp::float3       position = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(enemyEntity).position;

        //-------------------------------------------------------------
        // 湧いたときの補正倍率を、今持っている武器の素の値との比から逆算しておく。
        // 攻撃力は危険度×エリート、間合いはエリートの大きさだけが掛かっている
        //-------------------------------------------------------------
        const PaladinWeaponAttack& baseAttack = GetPaladinWeaponAttack(heldWeaponId);
        const float damageScale = registry.GetComponent<EnemyAttackHitboxComponent>(enemyEntity).damage / baseAttack.hitboxDamage;
        const float sizeScale   = registry.GetComponent<EnemyComponent>(enemyEntity).attackRange / baseAttack.attackRange;

        //-------------------------------------------------------------
        // 足す種類を決める。手持ち以外の種類を並べて混ぜ、そこから1本か全部を取る
        // （合計で2本か、全種類か）。個体ごとに組み合わせが変わるので、
        // どの武器を持っているかを見て身構え方を変える余地が出る
        //-------------------------------------------------------------
        std::vector<WeaponId> addIds;
        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i) {
            if(static_cast<WeaponId>(i) != heldWeaponId)
                addIds.push_back(static_cast<WeaponId>(i));
        }
        if(addIds.empty())
            return;

        std::shuffle(addIds.begin(), addIds.end(), rng);
        std::uniform_int_distribution<size_t> addCountDist(1, addIds.size());
        addIds.resize(addCountDist(rng));

        //-------------------------------------------------------------
        // 武器を作る。並びは種類の順にそろえる（肩の上の並びが個体ごとにばらつかないように）。
        // 武器を作るたびにWeaponComponentの格納先が動き得るので、参照は全部作り終えてから取り直す
        //-------------------------------------------------------------
        std::vector<Tsukino::ECS::Entity> weapons;
        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i) {
            const WeaponId id = static_cast<WeaponId>(i);
            if(id == heldWeaponId)
                weapons.push_back(heldWeaponEntity);
            else if(std::find(addIds.begin(), addIds.end(), id) != addIds.end())
                weapons.push_back(SpawnWeapon(registry, context, id, position, enemyEntity));
        }

        const int count = static_cast<int>(weapons.size());
        for(int i = 0; i < count; ++i) {
            WeaponComponent& weapon = registry.GetComponent<WeaponComponent>(weapons[static_cast<size_t>(i)]);
            weapon.floatEnabled     = true;
            weapon.floatSelected    = (weapons[static_cast<size_t>(i)] == heldWeaponEntity);    // 使っている1本だけ高く浮かせる
            weapon.localOffset      = hlslpp::float3((static_cast<float>(i) - (count - 1) * 0.5f) * GetEliteSettings().arsenalSpacing,
                                                     GetEliteSettings().arsenalHeight * sizeScale, GetEliteSettings().arsenalDepth);
        }

        PaladinArsenalComponent& arsenal = registry.AddComponent<PaladinArsenalComponent>(enemyEntity);
        arsenal.weaponEntities           = std::move(weapons);
        arsenal.damageScale              = damageScale;
        arsenal.sizeScale                = sizeScale;
    }

    //-------------------------------------------------------------
    //! @brief Paladinの持ち武器を差し替える
    //-------------------------------------------------------------
    void SwitchPaladinWeapon(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                             Tsukino::ECS::Entity enemyEntity, Tsukino::ECS::Entity weaponEntity) {
        auto* held    = registry.try_get<EnemyHeldWeaponComponent>(enemyEntity);
        auto* arsenal = registry.try_get<PaladinArsenalComponent>(enemyEntity);
        if(!held || !arsenal || !context.assetManager || held->weaponEntity == weaponEntity)
            return;
        if(!registry.IsValid(weaponEntity) || !registry.HasComponent<WeaponComponent>(weaponEntity))
            return;

        // 使う1本だけを高く浮かせて、どれに持ち替えたかを見せる
        for(Tsukino::ECS::Entity entity : arsenal->weaponEntities) {
            if(auto* weapon = registry.try_get<WeaponComponent>(entity)) {
                weapon->floatSelected = (entity == weaponEntity);
                weapon->isAttacking   = false;
            }
        }

        const WeaponId weaponId = registry.GetComponent<WeaponComponent>(weaponEntity).weaponId;
        held->weaponEntity      = weaponEntity;
        held->weaponId          = weaponId;

        //-------------------------------------------------------------
        // 攻撃モーション・間合い・判定を武器に合わせる。湧いたときの補正は掛け直す
        //-------------------------------------------------------------
        ApplyHeldWeaponAttack(registry, context, enemyEntity, weaponId, arsenal->sizeScale, arsenal->damageScale);
    }
}    // namespace CombatAndroid::ECS
