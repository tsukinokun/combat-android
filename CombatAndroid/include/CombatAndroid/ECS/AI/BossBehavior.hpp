//-------------------------------------------------------------
//! @file   BossBehavior.hpp
//! @brief  ボス用ビヘイビアツリー構築関数の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/Enemy/BehaviorTreeComponent.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  複数の技を持つボス用のビヘイビアツリーを構築する。
    //!         技の中身（クリップ・判定・予兆の形・距離・クールタイム）は全て
    //!         BossMoveSetComponent::key が指す技の表（Assets/Tables/Bosses/<key>.json）にあるので、
    //!         中ボス・ラスボスともにこの1種類のツリーを共有する
    //! @details
    //! Selector（記憶あり）
    //!  ├─ Sequence "Death" : 死亡していれば倒れて消える（ゾンビと共通）
    //!  └─ Action   "Boss"  : 追跡 → 技を選ぶ → 技（予兆 → 判定）→ 隙 を繰り返す。
    //!                         HPが閾値を切ったら激昂の技を出し、以後は速く・技の間隔を短くする
    //! @note   ボスはのけぞらない（EnemyComponent::superArmor）。のけぞりで技が潰れるとボスの攻めが成り立たないため
    //! @return 構築済みツリーのルートノード
    //-------------------------------------------------------------
    std::shared_ptr<EnemyBehaviorNode> BuildBossTree();
}    // namespace CombatAndroid::ECS
