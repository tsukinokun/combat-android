//-------------------------------------------------------------
//! @file   SoftTargetSystem.hpp
//! @brief  プレイヤーが狙う敵（ソフトターゲット）を自動で選ぶシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  SoftTargetSystem
    //! @brief  基準の向き（移動入力の向き、無ければキャラの正面）から±assistAngleDeg以内・selectRange以内の
    //!         生きている敵の中から、角度と距離の点数が一番良い敵をターゲットに選ぶ。
    //!         今のターゲットは点数をひいきし、乗り換えた直後はしばらく乗り換えないので、
    //!         敵が多くても印がちらちら移らない。振り向きが効く距離（assistRange）かどうかも決める
    //! @note   実行順はプレイヤーの位置と入力が確定するMovementの後、攻撃の段に入る
    //!         PlayerAnimationSystem（Gameplay）の前（SystemPriority::SoftTarget）
    //-------------------------------------------------------------
    class SoftTargetSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
