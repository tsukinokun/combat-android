//-------------------------------------------------------------
//! @file   GroundFollowSystem.hpp
//! @brief  GroundFollowSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  GroundFollowSystem
    //! @brief  GroundFollowComponentを持つエンティティ（地面）の位置(x/z)を、
    //!         毎フレームプレイヤーの位置(x/z)へ追従させるシステム。
    //!         Yは GroundFollowComponent::groundHeight で固定する。
    //! @note   プレイヤーは CharacterControllerComponent を持つ最初のエンティティを
    //!         追う（GrassFieldSystemのプレイヤー位置取得と同じ流儀）。
    //!
    //!         実行順序はプレイヤーのMovementが確定した後、かつPhysicsより前で
    //!         あること（詳細はSystemPriority.hpp参照）。ここで書いたTransformを
    //!         PhysicsSystemがKinematicボディの同期時に読むため、Physicsより後だと
    //!         1フレーム遅れて地面が追いついてくる
    //-------------------------------------------------------------
    class GroundFollowSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
