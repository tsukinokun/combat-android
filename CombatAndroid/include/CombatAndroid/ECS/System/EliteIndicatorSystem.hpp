//-------------------------------------------------------------
//! @file   EliteIndicatorSystem.hpp
//! @brief  EliteIndicatorSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EliteIndicatorSystem
    //! @brief  画面外にいるエリート敵の方向を、画面端の矢印（細い棒）で知らせるシステム。
    //! @note   対象が画面内に入っている間は何も出さない。エンティティは
    //!         EliteSettings::maxLiveElites分だけ初回Updateで遅延プールする
    //!         （HitImpactEffectSystemと同じ流儀）
    //-------------------------------------------------------------
    class EliteIndicatorSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        std::vector<Tsukino::ECS::Entity> m_indicatorEntities;    //!< 矢印のプール。初回Updateで生成
        float                              m_logTimer = 0.0f;      //!< 診断ログの間引き用（調整用。動作確認後は削除してよい）
    };
}    // namespace CombatAndroid::ECS
