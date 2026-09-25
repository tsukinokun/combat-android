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

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Core/Path.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 出現率。
        //
        // ★ エリートの出やすさを調整したいときはここの数値だけを触ればよい ★
        //
        // 危険度2（2分）から出始め、危険度が1上がるごとに1%ずつ増える（上限10%）。
        // 湧き間隔は中盤で約1秒なので、5%でおよそ1分に3体。ラスト1分は湧きも詰まるうえ
        // 率も1.5倍にして山場を作る。同時出現数はkMaxLiveElitesで頭打ちにする
        //-------------------------------------------------------------
        constexpr int   kFirstEliteRank       = 2;        //!< この危険度から出始める
        constexpr float kChanceAtFirstRank    = 0.03f;    //!< 出始めの確率
        constexpr float kChancePerRank        = 0.01f;    //!< 危険度1ごとに増える確率
        constexpr float kChanceMax            = 0.10f;    //!< 確率の上限
        constexpr float kFinalStretchChanceScale = 1.5f;  //!< ラスト1分の倍率

        //-------------------------------------------------------------
        // 強化の倍率。
        //
        // 見た目の大きさを変えるなら、体の当たり・攻撃の届く距離・攻撃判定の太さも
        // 同じだけ変えないと「大きいのに当たらない」「HPバーが体に埋まる」になる。
        // 怯み閾値の倍率は体力の倍率以下に保つこと（EnemyDifficultyTableと同じ約束：
        // 閾値だけ伸ばすと一撃で怯まない硬さが体力以上に跳ね上がる）
        //-------------------------------------------------------------
        constexpr float kSizeScale       = 1.3f;    //!< 見た目・体の当たり・攻撃範囲
        constexpr float kHealthScale     = 4.0f;    //!< 体力
        constexpr float kDamageScale     = 1.5f;    //!< 攻撃力
        constexpr float kThresholdScale  = 3.0f;    //!< 怯み閾値
        constexpr float kMoveSpeedScale  = 1.1f;    //!< 移動速度
        constexpr float kExpRewardScale  = 5.0f;    //!< 経験値

        static_assert(kThresholdScale <= kHealthScale, "エリートの怯み閾値の倍率は体力の倍率以下に保つこと");

        //-------------------------------------------------------------
        // エリートPaladinが肩の上に並べて浮かせる武器の配置（所有者のローカル空間）。
        // 高さは普通のPaladin（WeaponSpawnerの既定の浮遊位置170）を大きさの倍率で持ち上げ、
        // 横はプレイヤーの手持ち（PickupSystemのkFloatSpacing）と同じ考え方で等間隔に並べる
        //-------------------------------------------------------------
        constexpr float kArsenalSpacing = 80.0f;     //!< 隣り合う武器の横の間隔
        constexpr float kArsenalHeight  = 170.0f;    //!< 普通の大きさのときの浮遊の高さ
        constexpr float kArsenalDepth   = -30.0f;    //!< 前後（少し背中側）
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 今回湧かせる1体をエリートにするかを抽選する
    //-------------------------------------------------------------
    bool RollElite(std::mt19937& rng, int dangerRank, bool isFinalStretch, int liveEliteCount) {
        if(dangerRank < kFirstEliteRank || liveEliteCount >= kMaxLiveElites)
            return false;

        float chance = std::min(kChanceAtFirstRank + kChancePerRank * static_cast<float>(dangerRank - kFirstEliteRank), kChanceMax);
        if(isFinalStretch)
            chance *= kFinalStretchChanceScale;

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng) < chance;
    }

    //-------------------------------------------------------------
    //! @brief 生成設定をエリート用に強化する
    //-------------------------------------------------------------
    void ApplyEliteModifiers(EnemySpawnConfig& config) {
        config.sizeScale *= kSizeScale;

        config.healthScale *= kHealthScale;
        config.attackScale *= kDamageScale;
        config.knockbackThresholdScale *= kThresholdScale;
        config.moveSpeedScale *= kMoveSpeedScale;
        config.expScale *= kExpRewardScale;
    }

    //-------------------------------------------------------------
    //! @brief 出現ログに出す敵の呼び名
    //-------------------------------------------------------------
    const wchar_t* GetEliteDisplayName(EnemyTypeId id) {
        switch(id) {
        case EnemyTypeId::SmallZombie: return L"ゾンビ";
        case EnemyTypeId::BigZombie:   return L"大ゾンビ";
        case EnemyTypeId::Paladin:     return L"パラディン";
        default:                       return L"敵";
        }
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
            weapon.localOffset      = hlslpp::float3((static_cast<float>(i) - (count - 1) * 0.5f) * kArsenalSpacing,
                                                     kArsenalHeight * sizeScale, kArsenalDepth);
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
