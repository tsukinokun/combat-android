//-------------------------------------------------------------
//! @file   SkillAuraComponent.hpp
//! @brief  SkillAuraComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct SkillAuraComponent
    //! @brief  取得スキル（七つの大罪）の色でプレイヤーが纏うオーラの実行時状態
    //! @note   既定値で始まる実行時状態だけを持つ（Prefabでは "null" でアタッチする）。
    //!         見た目はRimGlowComponent（輪郭の発光）で出し、それをブルームが滲ませる。
    //!         色と強さを直接切り替えると、スキルを取った瞬間にパッと変わって見えるため、
    //!         目標値へ毎フレーム寄せた「今の値」をここに持つ
    //-------------------------------------------------------------
    struct SkillAuraComponent {
        hlslpp::float3 currentColor    = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 今表示している色（目標の色へ寄せていく）
        float          currentStrength = 0.0f;                                 //!< 今の強さ（0=消灯、1=レベルに応じた全力）
        float          currentLevelT   = 0.0f;                                 //!< 今のレベル補間値（0=Lv1、1=最大レベル）
        bool           slowGlowing     = false;                                //!< 鈍足の白い縁を点けているか（消すときに使う）
        float          pulseTime       = 0.0f;                                 //!< 脈動用に経過時間を積算するだけの内部状態
    };
}    // namespace CombatAndroid::ECS
