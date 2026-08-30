//-------------------------------------------------------------
//! @file   EnemyHeldWeaponComponent.hpp
//! @brief  EnemyHeldWeaponComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <entt/entt.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EnemyHeldWeaponComponent
    //! @brief  敵が手に持っている武器エンティティへの参照。
    //!         武器本体は敵の子エンティティではなく独立したエンティティで、
    //!         CombatSystemがWeaponComponent::ownerを見て手ボーンへ追従させている
    //!         （プレイヤーの武器とまったく同じ経路）。
    //! @note   このコンポーネントを持つ敵を破棄するときは、必ず武器エンティティの
    //!         始末も併せて行うこと。行き先は2通りあり、どちらも取りこぼすと
    //!         所有者を失った武器がワールドに residual として残り続ける：
    //!           - 撃破された（BTのPlayDeath）        → EnemyDiedEventで運び、
    //!                                                   EnemyWeaponDropSystemが地面へ落とす
    //!           - 遠すぎて間引かれた（CullDistantEnemies）→ 武器も一緒にQueueDestroyする
    //-------------------------------------------------------------
    struct EnemyHeldWeaponComponent {
        Tsukino::ECS::Entity weaponEntity = entt::null;           //!< 手に持っている武器エンティティ
        WeaponId             weaponId     = WeaponId::Warhammer;    //!< その武器の種類（ドロップ時の表示名の解決に使う）
    };
}    // namespace CombatAndroid::ECS
