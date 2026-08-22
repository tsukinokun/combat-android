//-------------------------------------------------------------
//! @file   PlayerSystem.hpp
//! @brief  PlayerSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  PlayerSystem
    //! @brief  プレイヤーの入力を読み取り、CharacterControllerComponentへ移動を、
    //!         PlayerComponentへ攻撃・回避の生入力を書き込むシステム
    //-------------------------------------------------------------
    class PlayerSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
