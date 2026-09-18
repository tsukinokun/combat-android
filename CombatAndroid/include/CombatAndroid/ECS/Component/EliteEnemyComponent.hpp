//-------------------------------------------------------------
//! @file   EliteEnemyComponent.hpp
//! @brief  EliteEnemyComponent構造体の宣言
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/EnemySpawnTable.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EliteEnemyComponent
    //! @brief  エリート（強化個体）であることを示すタグ
    //! @note   数値の強化は生成前の設定（ApplyEliteModifiers）で済んでいるので、ここは目印と元の種類だけ。
    //!         （中身の無い型はRegistry::AddComponentが参照を返せないので、種類を持たせてある）
    //!         EnemyAttackTelegraphSystemが常時の紫の発光を、ZombieBehaviorが撃破通知の
    //!         isEliteを、EnemySpawnDirectorSystemが同時出現数の上限をこれで判定する
    //-------------------------------------------------------------
    struct EliteEnemyComponent {
        EnemyTypeId baseType = EnemyTypeId::SmallZombie;    //!< 元になった敵の種類
    };
}    // namespace CombatAndroid::ECS
