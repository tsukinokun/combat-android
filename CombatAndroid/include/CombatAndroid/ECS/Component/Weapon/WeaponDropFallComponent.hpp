//-------------------------------------------------------------
//! @file   WeaponDropFallComponent.hpp
//! @brief  WeaponDropFallComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct WeaponDropFallComponent
    //! @brief  撃破された敵の手を離れ、地面へ落ちている最中の武器に付与するコンポーネント。
    //!         BeginWeaponDropが付け、EnemyWeaponDropSystemが毎フレーム
    //!         start→groundの姿勢へ補間して、着地したらDropWeaponToWorldで
    //!         拾える状態にしてから外す（＝落下終了の合図）
    //! @note   落下中はPickupComponentを持たないため拾えない
    //-------------------------------------------------------------
    struct WeaponDropFallComponent {
        hlslpp::float3     startPosition  = hlslpp::float3(0.0f, 0.0f, 0.0f);          //!< 手を離れた瞬間の位置
        hlslpp::quaternion startRotation  = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    //!< 手を離れた瞬間の姿勢
        hlslpp::float3     groundPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);          //!< 着地位置（yは接地高さ）
        hlslpp::quaternion groundRotation = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    //!< 着地姿勢（横たわり）
        float              timer          = 0.0f;                                          //!< 落下開始からの経過時間
    };
}    // namespace CombatAndroid::ECS
