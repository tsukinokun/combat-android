//-------------------------------------------------------------
//! @file   EnemyBehaviorSystem.hpp
//! @brief  EnemyBehaviorSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyBehaviorSystem
    //! @brief  BehaviorTreeComponentを持つ敵（全敵）の黒板を更新し、ビヘイビアツリーを進行させるシステム
    //-------------------------------------------------------------
    class EnemyBehaviorSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
