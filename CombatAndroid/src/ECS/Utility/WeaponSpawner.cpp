//-------------------------------------------------------------
//! @file    WeaponSpawner.cpp
//! @brief   武器エンティティの生成処理の実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>

#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponDropFallComponent.hpp>
#include <CombatAndroid/ECS/Serialization/WeaponComponentSerialization.hpp>
#include <CombatAndroid/ECS/Utility/GamePrefab.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/ECS/Prefab/PrefabFactory.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Serialization/TransformComponentSerialization.hpp>

#include <entt/entt.hpp>

#include <array>
#include <iterator>
#include <optional>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 武器のPrefab名（WeaponIdの並び順）。
        // ★ 武器を1種追加するときは、WeaponIdへ足し、WeaponTable.cppとここへ1行ずつ足し、Prefabを1つ置く ★
        //-------------------------------------------------------------
        constexpr const char* kWeaponPrefabNames[] = {
            "Weapon/Warhammer",
            "Weapon/Greatsword",
            "Weapon/Battleaxe",
        };

        // 種類を足したのにPrefab名を書き忘れる事故を防ぐ（WeaponTable.cppと同じ作法）
        static_assert(std::size(kWeaponPrefabNames) == static_cast<size_t>(WeaponId::Count),
                      "WeaponId に種類を足したら kWeaponPrefabNames にも1行足すこと");

        //-------------------------------------------------------------
        //! @struct WeaponPrefabDefaults
        //! @brief  武器Prefabが持つ「落ちている状態」の値。持ち主の手から離れたときに戻す先
        //-------------------------------------------------------------
        struct WeaponPrefabDefaults {
            WeaponComponent                            weapon;      //!< 持ち方（肩の斜め上に置く既定の追従パラメータ）
            Tsukino::BuiltIn::ECS::TransformComponent ground;      //!< 地面に横たわる姿勢
            PickupComponent                            pickup;      //!< 拾える範囲・表示名
        };

        //-------------------------------------------------------------
        //! @brief  武器PrefabのComponent JSONから「落ちている状態」の値を引く関数
        //! @note   初回に1度だけ読んで覚える（PrefabFactoryと同じくメインスレッドからだけ呼ぶ）
        //-------------------------------------------------------------
        const WeaponPrefabDefaults& GetWeaponPrefabDefaults(Tsukino::EngineIntegration::EngineContext& context, WeaponId id) {
            static std::array<std::optional<WeaponPrefabDefaults>, static_cast<size_t>(WeaponId::Count)> s_defaults;

            int index = static_cast<int>(id);
            if(index < 0 || index >= static_cast<int>(WeaponId::Count))
                index = 0;

            std::optional<WeaponPrefabDefaults>& slot = s_defaults[static_cast<size_t>(index)];
            if(!slot) {
                WeaponPrefabDefaults defaults;
                const std::string    directory = GetPrefabDirectory(kWeaponPrefabNames[index]);
                (void)context.prefabFactory->Load(directory + "/WeaponComponent.json", "WeaponComponent", defaults.weapon);
                (void)context.prefabFactory->Load(directory + "/TransformComponent.json", "TransformComponent", defaults.ground);
                (void)context.prefabFactory->Load(directory + "/PickupComponent.json", "PickupComponent", defaults.pickup);
                slot = std::move(defaults);
            }
            return *slot;
        }

        //-------------------------------------------------------------
        //! @brief  所有者に持たれるときの追従パラメータをPrefabの値へ書き戻すヘルパー
        //! @param  weapon   [in,out] 対象のコンポーネント
        //! @param  defaults [in]     Prefabの値
        //! @note   持ち主の手から離れるとき（DropWeaponToWorld）に呼ぶ。落とした武器を次に拾った者が
        //!         「前の持ち主の持ち方」を引き継いでしまわないようにするための一本化で、実際これを戻し忘れて
        //!         「敵から拾った武器だけプレイヤーの手ボーンに張り付いて暴れる」不具合を出している
        //-------------------------------------------------------------
        void ApplyDefaultCarryPose(WeaponComponent& weapon, const WeaponComponent& defaults) {
            weapon.localOffset              = defaults.localOffset;
            weapon.gripRotationOffset       = defaults.gripRotationOffset;
            weapon.handTrackingWeight       = defaults.handTrackingWeight;
            weapon.attackHandTrackingWeight = defaults.attackHandTrackingWeight;
            weapon.isAttacking              = false;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 武器の種類からPrefab名を引く
    //-------------------------------------------------------------
    const char* GetWeaponPrefabName(WeaponId id) {
        int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(WeaponId::Count))
            index = 0;

        return kWeaponPrefabNames[index];
    }

    //-------------------------------------------------------------
    //! @brief 武器エンティティを1つ生成する
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnWeapon(Tsukino::ECS::Registry& registry,
                                     Tsukino::EngineIntegration::EngineContext& context,
                                     WeaponId weaponId,
                                     const hlslpp::float3& position,
                                     Tsukino::ECS::Entity owner) {
        Tsukino::ECS::Entity weaponEntity = InstantiatePrefab(registry, context, GetWeaponPrefabName(weaponId));

        Tsukino::BuiltIn::ECS::TransformComponent& transform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity);
        transform.position = position;
        // 所有者がいる場合の姿勢は次のフレームからCombatSystemが上書きするため、Prefabの「横たわった」姿勢は使わない
        if(owner != entt::null)
            transform.rotation = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        transform.dirty = true;

        WeaponComponent& weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
        weapon.owner            = owner;
        weapon.level            = 1;
        weapon.floatEnabled     = false;    // 浮遊演出が要る場合は呼び出し側で立てる
        // ばね追従の状態は次のCombatSystem更新で所有者の定位置へ置き直させる
        // （ここではまだownerも位置も決まっていないため）
        weapon.hasFollowSpringState = false;

        // 実効ステータス（damage）は常にWeaponTableから導出する（加算では積み上げない）
        RecalculateWeaponStats(weapon);

        // Prefabは「ワールドに落ちている＝拾える」状態。所有者がいれば拾える部品を外す
        if(owner != entt::null && registry.HasComponent<PickupComponent>(weaponEntity))
            registry.RemoveComponent<PickupComponent>(weaponEntity);

        return weaponEntity;
    }

    //-------------------------------------------------------------
    //! @brief 所有者の手から外し、その場に落ちているピックアップへ戻す
    //-------------------------------------------------------------
    void DropWeaponToWorld(Tsukino::ECS::Registry& registry,
                           Tsukino::EngineIntegration::EngineContext& context,
                           Tsukino::ECS::Entity weaponEntity,
                           const hlslpp::float3& groundPosition) {
        // 敵が間引かれた等で武器が既に破棄されている場合に備えて存在を確かめる
        // （HasComponentは無効なエンティティに対して呼べない）
        if(!registry.IsValid(weaponEntity) || !registry.HasComponent<WeaponComponent>(weaponEntity))
            return;

        WeaponComponent&            weapon   = registry.GetComponent<WeaponComponent>(weaponEntity);
        const WeaponPrefabDefaults& defaults = GetWeaponPrefabDefaults(context, weapon.weaponId);

        // 未所有＝CombatSystemの追従処理（owner != entt::nullが条件）に入らなくなり、その場に留まる
        weapon.owner        = entt::null;
        weapon.floatEnabled = false;
        weapon.isSnapped    = false;
        weapon.attackBlend  = 0.0f;
        // 次に拾われたとき、地面に落ちていた位置からばねで引っ張り上げられるのではなく
        // 浮遊の定位置から始まるように状態を捨てる
        weapon.hasFollowSpringState = false;
        // 追従が止まる以上、前フレーム姿勢からのスイープ判定も無効にしておく
        weapon.hasPrevAttackPose = false;

        // 持ち方をPrefabの値へ戻す。前の持ち主（敵）が握りパラメータを変えていた場合、
        // ここで戻さないと次に拾ったプレイヤーがその持ち方を引き継いでしまう
        ApplyDefaultCarryPose(weapon, defaults.weapon);

        if(registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity)) {
            Tsukino::BuiltIn::ECS::TransformComponent& transform =
                registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity);
            transform.position = groundPosition;
            transform.rotation = defaults.ground.rotation;    // 手置きの武器と同じく地面に横たわらせる
            transform.dirty    = true;
        }

        // 既にPickupComponentを持っている（＝二重にドロップされた）場合は付け直さない。
        // AddComponentし直すと拾いかけのリムグローの状態が飛んでしまう
        if(!registry.HasComponent<PickupComponent>(weaponEntity))
            registry.AddComponent<PickupComponent>(weaponEntity, defaults.pickup);
    }

    //-------------------------------------------------------------
    //! @brief 所有者の手から外し、今の姿勢から地面へ落ち始めさせる
    //-------------------------------------------------------------
    void BeginWeaponDrop(Tsukino::ECS::Registry& registry,
                         Tsukino::EngineIntegration::EngineContext& context,
                         Tsukino::ECS::Entity weaponEntity,
                         const hlslpp::float3& groundPosition) {
        // DropWeaponToWorldと同じく、敵が間引かれた等で既に破棄されている場合に備える
        if(!registry.IsValid(weaponEntity) || !registry.HasComponent<WeaponComponent>(weaponEntity)
           || !registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity))
            return;

        // 二重にドロップされた場合は、落下を最初からやり直さない
        if(registry.HasComponent<WeaponDropFallComponent>(weaponEntity))
            return;

        // 未所有にして、次のCombatSystem更新から手ボーンへの追従を止める。
        // 持ち方のリセットやPickupComponentの付与は着地時のDropWeaponToWorldに任せる
        WeaponComponent& weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
        weapon.owner            = entt::null;

        const WeaponPrefabDefaults& defaults = GetWeaponPrefabDefaults(context, weapon.weaponId);

        const Tsukino::BuiltIn::ECS::TransformComponent& transform =
            registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity);

        WeaponDropFallComponent fall;
        fall.startPosition  = transform.position;
        fall.startRotation  = transform.rotation;
        fall.groundPosition = groundPosition;
        fall.groundRotation = defaults.ground.rotation;    // DropWeaponToWorldが着地時に書く姿勢と揃える
        fall.timer          = 0.0f;
        registry.AddComponent<WeaponDropFallComponent>(weaponEntity, fall);
    }
}    // namespace CombatAndroid::ECS
