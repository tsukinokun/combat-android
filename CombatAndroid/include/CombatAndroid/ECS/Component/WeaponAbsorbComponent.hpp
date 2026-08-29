//-------------------------------------------------------------
//! @file   WeaponAbsorbComponent.hpp
//! @brief  WeaponAbsorbComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct WeaponAbsorbComponent
    //! @brief  レベルアップの糧になった武器（拾った側）に付与し、装備中の同種武器へ
    //!         吸い寄せている間だけ存在するコンポーネント。PickupSystemが毎フレーム
    //!         targetへ向けて移動させ、到達したらレベル加算・リムライト発光を行って
    //!         このコンポーネントを外す（＝吸い寄せ終了の合図）
    //-------------------------------------------------------------
    struct WeaponAbsorbComponent {
        Tsukino::ECS::Entity target     = entt::null;    //!< 吸い寄せ先（装備中の同種武器エンティティ）
        float                 stateTimer = 0.0f;           //!< 吸い寄せ開始からの経過時間（加速イージング・タイムアウト用）
    };
}    // namespace CombatAndroid::ECS
