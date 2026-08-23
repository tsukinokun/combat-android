//-------------------------------------------------------------
//! @file   BehaviorTreeComponent.hpp
//! @brief  BehaviorTreeComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/BehaviorTree.hpp>
#include <CombatAndroid/ECS/Component/EnemyBlackboard.hpp>

#include <memory>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! @brief 敵のビヘイビアツリーが使うノード型（黒板はEnemyBlackboard固定）
    using EnemyBehaviorNode = BehaviorNode<EnemyBlackboard>;

    //-------------------------------------------------------------
    //! @struct BehaviorTreeComponent
    //! @brief  ビヘイビアツリー駆動の敵であることを表すコンポーネント（現状、全敵が持つ）
    //-------------------------------------------------------------
    struct BehaviorTreeComponent {
        //-------------------------------------------------------------
        // ツリーはComposite内にRunning再開位置（m_runningIndex）を持つため、
        // 敵ごとに専用のインスタンスを1本持つ（他エンティティと共有しない）
        //-------------------------------------------------------------
        std::shared_ptr<EnemyBehaviorNode> root;             //!< ツリーのルートノード
        EnemyBlackboard                    blackboard;        //!< このエンティティ専用の黒板
    };
}    // namespace CombatAndroid::ECS
