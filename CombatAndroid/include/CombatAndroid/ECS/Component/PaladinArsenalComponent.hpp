//-------------------------------------------------------------
//! @file   PaladinArsenalComponent.hpp
//! @brief  PaladinArsenalComponent構造体の宣言
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <entt/entt.hpp>

#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PaladinArsenalComponent
    //! @brief  複数（2本以上。どの種類を持つかは個体ごとに違う）の武器を持ち、
    //!         攻撃ごとに持ち替えるPaladin（エリート）の手持ち一覧
    //! @note   使っている1本はEnemyHeldWeaponComponent::weaponEntityが指し、ここには
    //!         それも含めた全部を持つ。使っていない武器は肩の上に浮かせておき、
    //!         PaladinWeaponSwitchSystemが攻撃の終わりに次の1本を選んで差し替える。
    //!         撃破されたら全部落とし（EnemyWeaponDropSystem）、間引かれたら全部消す
    //-------------------------------------------------------------
    struct PaladinArsenalComponent {
        std::vector<Tsukino::ECS::Entity> weaponEntities;    //!< 持っている武器（使用中の1本を含む）

        //-------------------------------------------------------------
        // 持ち替えても危険度・エリートの補正が消えないよう、湧いたときの補正倍率を覚えておく。
        // 持ち替え時はGetPaladinWeaponAttackの素の値にこれを掛け直す
        //-------------------------------------------------------------
        float damageScale = 1.0f;    //!< 攻撃力の倍率（危険度×エリート）
        float sizeScale   = 1.0f;    //!< 間合い・判定の太さの倍率（エリートの大きさ）

        EnemyAnimState previousState = EnemyAnimState::Idle;    //!< 前フレームのステート（攻撃の終わりを検出する）
        int            sameWeaponStreak = 1;                     //!< 同じ武器を続けて選んだ回数（単調にならないよう制限する）
    };
}    // namespace CombatAndroid::ECS
