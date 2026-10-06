//-------------------------------------------------------------
//! @file   EnemyAttackTelegraphSystem.hpp
//! @brief  EnemyAttackTelegraphSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyAttackTelegraphSystem
    //! @brief  敵が攻撃を振りかぶっている間、赤いリムライトで予兆を出すシステム
    //! @note   敵は攻撃モーションへ入ってから EnemyAttackHitboxComponent::hitStartTime 秒後に
    //!         判定が出る。その手前を「振りかぶり」として光らせ、判定が出る瞬間に向かって
    //!         強くしていくことで、回避（Space）を間に合わせられるようにする。
    //!
    //!         EnemyAnimationSystemが今フレームのcurrentStateを確定させた後に読む必要があるため、
    //!         同じGameplay優先度で、EnemyAnimationSystemより後に登録する
    //-------------------------------------------------------------
    class EnemyAttackTelegraphSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（予兆の進行は攻撃モーションの経過時間で決まるため使わない）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
