//-------------------------------------------------------------
//! @file   WeaponLevelDebugSystem.hpp
//! @brief  WeaponLevelDebugSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  WeaponLevelDebugSystem
    //! @brief  プレイヤーの所持武器（PlayerComponent::weaponInventory）ごとに、
    //!         種類名とレベルを一覧表示するデバッグ専用HUDシステム。
    //!         トグルキーは持たず、対象エンティティ（WeaponLevelDebugComponent）が
    //!         存在する間は常に表示する。
    //!         対象エンティティの生成・本Systemのシーンへの登録はいずれも
    //!         _DEBUGビルドでのみ行われる（CombatAndroidScene.cpp参照）ため、
    //!         Releaseビルドでは実質的に何も表示されない
    //-------------------------------------------------------------
    class WeaponLevelDebugSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
