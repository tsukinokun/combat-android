//-------------------------------------------------------------
//! @file   ProjectileSystem.hpp
//! @brief  ProjectileSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  ProjectileSystem
    //! @brief  ProjectileComponentを持つ飛翔体（バトルアックスの斬撃弾）を進め、
    //!         通過した敵へダメージを与えるシステム。
    //!         貫通する弾は寿命または飛距離で、貫通しない弾（溜めの浅いもの）は
    //!         最初にダメージを与えた1体で破棄される
    //! @note   ヒットの確定はCombatSystemと共有のApplyCombatHit（CombatHit.hpp）を通す。
    //!         見た目のエフェクトはEffectComponentが受け持ち、EffectSystemが
    //!         このエンティティのTransformへ毎フレーム追従させる
    //-------------------------------------------------------------
    class ProjectileSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
