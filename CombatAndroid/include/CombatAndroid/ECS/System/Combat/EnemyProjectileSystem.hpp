//-------------------------------------------------------------
//! @file   EnemyProjectileSystem.hpp
//! @brief  EnemyProjectileSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyProjectileSystem
    //! @brief  敵の弾（EnemyProjectileComponent）を進め、プレイヤーに当たったらダメージと鈍足を与えて消すシステム
    //! @note   当てる処理は体の判定と共通（CombatHit.cppのDamagePlayer）。無敵中はすり抜けて飛び続ける。
    //!         弾の生成はBossDirectorSystem（BossProjectileEventを受ける）が行う
    //-------------------------------------------------------------
    class EnemyProjectileSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
