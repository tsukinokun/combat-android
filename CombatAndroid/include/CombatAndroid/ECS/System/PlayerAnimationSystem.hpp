//-------------------------------------------------------------
//! @file   PlayerAnimationSystem.hpp
//! @brief  PlayerAnimationSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <CombatAndroid/ECS/Utility/StateMachine.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  PlayerAnimationSystem
    //! @brief  プレイヤーの状態（移動/スプリント/攻撃・回避入力）を見て、
    //!         Idle/Run/FastRun/Dodge/Attack1-3のアニメーションステートマシンを進行させるシステム
    //-------------------------------------------------------------
    class PlayerAnimationSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief コンストラクタ。各ステートのOnEnterコールバック（クリップ切り替え）を登録する
        //-------------------------------------------------------------
        PlayerAnimationSystem();

        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        StateMachine<PlayerAnimState> m_stateMachine;    //!< Idle/Run/FastRun/Dodge/Attack1-3の遷移とクリップ切り替えを管理する
    };
}    // namespace CombatAndroid::ECS
