//-------------------------------------------------------------
//! @file    WeaponSpawner.hpp
//! @brief   武器エンティティの生成処理の宣言
//! @author  山﨑愛
//! @note    武器1種類の「見た目と振り方」（メッシュ・表示名・握り・専用攻撃モーション・AoE・
//!          ノックバック・溜め攻撃・斬撃弾）は Assets/Prefabs/Weapon/<名前>/ のPrefabが持つ。
//!          手置き・敵の持ち物・死亡ドロップのどの経路もSpawnWeaponでPrefabから作るので、
//!          出所によらずまったく同じ性能になる。レベルごとの攻撃力はWeaponTableが持つ
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

#include <Tsukino/Core/ECS/Registry/Registry.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <hlsl++.h>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  武器の種類からPrefab名を引く関数
    //! @param  id [in] 武器の種類
    //! @return Assets/Prefabs/ からの相対名（例: "Weapon/Warhammer"）
    //-------------------------------------------------------------
    [[nodiscard]]
    const char* GetWeaponPrefabName(WeaponId id);

    //-------------------------------------------------------------
    //! @brief  武器エンティティを1つ生成する関数
    //! @param  registry [in]     エンティティレジストリ
    //! @param  context  [in]     エンジンコンテキスト
    //! @param  weaponId [in]     生成する武器の種類
    //! @param  position [in]     初期位置（ownerを指定した場合は次のフレームに追従位置へ上書きされる）
    //! @param  owner    [in]     所有者。entt::nullなら地面に落ちている状態で作る
    //! @return 生成した武器エンティティ
    //! @note   Prefabは「地面に落ちていて拾える」状態（横たわった姿勢＋PickupComponent）。
    //!         ownerを指定した場合はPickupComponentを外し、所有者の手ボーンへ追従する状態にする。
    //!         浮遊演出（floatEnabled）は呼び出し側で必要に応じて立てること
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnWeapon(Tsukino::ECS::Registry& registry,
                                     Tsukino::EngineIntegration::EngineContext& context,
                                     WeaponId weaponId,
                                     const hlslpp::float3& position,
                                     Tsukino::ECS::Entity owner = entt::null);

    //-------------------------------------------------------------
    //! @brief  所有者の手から外し、その場に落ちているピックアップへ戻す関数
    //! @param  registry       [in]     エンティティレジストリ
    //! @param  context        [in]     エンジンコンテキスト（Prefabの値を引くのに使う）
    //! @param  weaponEntity   [in]     対象の武器エンティティ
    //! @param  groundPosition [in]     落とす位置（yは接地高さへ差し替えてから渡すこと）
    //! @note   Paladinの死亡ドロップで使う。エンティティを作り直さずに所有状態だけ
    //!         戻すため、性能はそのまま引き継がれる。持ち方・横たわる姿勢・拾える表示はPrefabの値へ戻す。
    //!         PickupComponentを足す＝コンポーネント構成が変わるので、
    //!         必ずViewの反復の外側から呼ぶこと
    //-------------------------------------------------------------
    void DropWeaponToWorld(Tsukino::ECS::Registry& registry,
                           Tsukino::EngineIntegration::EngineContext& context,
                           Tsukino::ECS::Entity weaponEntity,
                           const hlslpp::float3& groundPosition);

    //-------------------------------------------------------------
    //! @brief  所有者の手から外し、今の姿勢から地面へ落ち始めさせる関数
    //! @param  registry       [in]     エンティティレジストリ
    //! @param  context        [in]     エンジンコンテキスト（着地姿勢をPrefabから引くのに使う）
    //! @param  weaponEntity   [in]     対象の武器エンティティ
    //! @param  groundPosition [in]     着地位置（yは接地高さへ差し替えてから渡すこと）
    //! @note   ownerを外してWeaponDropFallComponentを付けるだけで、位置はまだ動かさない。
    //!         落下の補間と着地時のDropWeaponToWorldはEnemyWeaponDropSystemが行う。
    //!         コンポーネント構成が変わるので、必ずViewの反復の外側から呼ぶこと
    //-------------------------------------------------------------
    void BeginWeaponDrop(Tsukino::ECS::Registry& registry,
                         Tsukino::EngineIntegration::EngineContext& context,
                         Tsukino::ECS::Entity weaponEntity,
                         const hlslpp::float3& groundPosition);
}    // namespace CombatAndroid::ECS
