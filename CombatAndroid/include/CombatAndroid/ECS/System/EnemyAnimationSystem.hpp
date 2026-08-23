//-------------------------------------------------------------
//! @file   EnemyAnimationSystem.hpp
//! @brief  EnemyAnimationSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <CombatAndroid/ECS/Utility/StateMachine.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyAnimationSystem
    //! @brief  EnemyAnimationSetComponent::desiredState（EnemyBehaviorSystemが書き込む）を見て、
    //!         Idle/Walk/Attack/Knockback/Deathのアニメーションステートマシンを進行させるシステム
    //-------------------------------------------------------------
    class EnemyAnimationSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief コンストラクタ。各ステートのOnEnterコールバック（クリップ切り替え）を登録する
        //-------------------------------------------------------------
        EnemyAnimationSystem();

        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        StateMachine<EnemyAnimState> m_stateMachine;    //!< Idle/Walk/Attack/Knockback/Deathの遷移とクリップ切り替えを管理する
    };
}    // namespace CombatAndroid::ECS
