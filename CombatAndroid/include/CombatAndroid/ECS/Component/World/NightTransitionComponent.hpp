//-------------------------------------------------------------
//! @file   NightTransitionComponent.hpp
//! @brief  NightTransitionComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum  NightState
    //! @brief 昼夜の切り替えの状態
    //-------------------------------------------------------------
    enum class NightState : int {
        Day = 0,      //!< 昼（色調補正なし）
        Nightfall,    //!< 球が広がって夜になっていく途中
        Night,        //!< 夜（球が世界を覆いきっている）
        Daybreak,     //!< 夜が明けていく途中
    };

    //-------------------------------------------------------------
    //! @struct NightTransitionComponent
    //! @brief  ボス戦の夜への切り替えの状態。SphereColorGradeComponent（エンジンの球形の色調補正）と
    //!         同じエンティティに1つだけ付け、NightTransitionSystemだけが書き込む
    //! @note   始めるのは RequestNightfall / RequestDaybreak（NightTransitionSystem.hpp）
    //-------------------------------------------------------------
    struct NightTransitionComponent {
        NightState     state   = NightState::Day;
        float          elapsed = 0.0f;                              //!< 今の状態に入ってからの秒（実時間）
        hlslpp::float3 center  = hlslpp::float3(0.0f, 0.0f, 0.0f);  //!< 夜が広がり始める中心

        //-----------------------------------------------------
        // 昼の太陽・空・霧の値。夜へ入るときに控え、明けるときに戻す
        //-----------------------------------------------------
        bool           dayCaptured        = false;
        float          daySunIntensity    = 1.0f;
        hlslpp::float3 daySunColor        = hlslpp::float3(1.0f, 1.0f, 1.0f);
        hlslpp::float3 daySunDirection    = hlslpp::float3(0.0f, -1.0f, 0.0f);
        float          daySkySunIntensity = 20.0f;
        hlslpp::float3 dayFogSunColor     = hlslpp::float3(1.0f, 1.0f, 1.0f);

        float lightBlend = 0.0f;    //!< 太陽・空・霧の、昼(0)から夜(1)への寄せ具合
    };
}    // namespace CombatAndroid::ECS
