//-------------------------------------------------------------
//! @file   CombatSystem.hpp
//! @brief  CombatSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  CombatSystem
    //! @brief  武器の当たり判定の有効化・追従、ダメージ処理、死亡判定を行うシステム。
    //!         武器→敵の判定はPhysicsSystem::OverlapCapsule（Jolt物理へのクエリ）で行うが、
    //!         敵→プレイヤーの判定はJolt物理を経由せず、プレイヤーのCharacterControllerComponent
    //!         （実際に動いている当たり判定そのもの）から直接カプセルを組み立てて幾何判定する
    //!         （プレイヤーはCharacterVirtualで駆動されておりJoltのBodyを持たないため、
    //!           物理クエリに依存すると1フレーム遅延やボディ未生成で判定が抜けやすい）
    //-------------------------------------------------------------
    class CombatSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
