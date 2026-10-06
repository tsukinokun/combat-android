//-------------------------------------------------------------
//! @file   PickupIndicatorSystem.hpp
//! @brief  PickupIndicatorSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  PickupIndicatorSystem
    //! @brief  画面外にある拾得アイテム（PickupComponent。今は落ちている武器のみ）の方向を、
    //!         画面端の矢印（細い棒）で知らせるシステム。
    //! @note   EliteIndicatorSystemと同じ手法（プロジェクションと画面端クランプ）。
    //!         同時に落ちている数の上限が無いため、矢印のプールは必要な数まで
    //!         増やしていく（EliteIndicatorSystemは上限が決まっているため固定数）
    //-------------------------------------------------------------
    class PickupIndicatorSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        std::vector<Tsukino::ECS::Entity> m_indicatorEntities;    //!< 矢印のプール。必要な数まで増やす
    };
}    // namespace CombatAndroid::ECS
