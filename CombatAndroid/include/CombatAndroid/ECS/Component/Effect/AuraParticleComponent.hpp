//-------------------------------------------------------------
//! @file   AuraParticleComponent.hpp
//! @brief  AuraParticleComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! @brief 同時に表示できるオーラの粒の最大数（＝プールするエンティティ数）
    constexpr int kAuraParticlePoolSize = 96;

    //-------------------------------------------------------------
    //! @struct AuraParticleComponent
    //! @brief  プレイヤーの足元から立ちのぼるオーラの粒1個分の状態
    //! @note   EXP玉と同じく、シーン生成時に作ったkAuraParticlePoolSize個を
    //!         SkillAuraParticleSystemが使い回す（実行時の生成・破棄は行わない）。
    //!         位置はプレイヤーの足元からの相対で持つ。走っても粒が置き去りにならず、
    //!         体に付いてくるオーラに見せるため
    //-------------------------------------------------------------
    struct AuraParticleComponent {
        bool           active      = false;                                 //!< 使用中か（falseならプールの空き）
        hlslpp::float3 localOffset = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< プレイヤーの足元からの相対位置
        float          riseSpeed   = 0.0f;                                  //!< 上昇速度（cm/秒）
        float          age         = 0.0f;                                  //!< 湧いてからの経過時間（秒）
        float          lifetime    = 1.0f;                                  //!< 寿命（秒）
        float          swayPhase   = 0.0f;                                  //!< 横揺れの位相（粒ごとにずらす）
        float          size        = 1.0f;                                  //!< 湧いたときの大きさ（TransformComponent.scaleの基準）
    };
}    // namespace CombatAndroid::ECS
