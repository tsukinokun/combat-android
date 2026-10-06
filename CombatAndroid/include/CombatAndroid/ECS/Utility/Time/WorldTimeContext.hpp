//-------------------------------------------------------------
//! @file    WorldTimeContext.hpp
//! @brief   そのフレームの時間の情報（レジストリのコンテキスト）
//-------------------------------------------------------------
#pragma once

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct WorldTimeContext
    //! @brief  CombatAndroidSceneが毎フレーム、Sceneを更新する前にレジストリのコンテキストへ置く。
    //!         システムが受け取るdeltaTimeには大技のスロー（SlowMotionController）とスキル選択の
    //!         停止が掛かっているので、スローに引きずられたくない演出（カメラの寄り等）は
    //!         ここから実時間を読む
    //-------------------------------------------------------------
    struct WorldTimeContext {
        float realDeltaTime = 0.0f;    //!< スロー・停止を掛ける前の経過秒
        float timeScale     = 1.0f;    //!< システムへ渡したdeltaTimeに掛かっている倍率（スキル選択中は0）
    };
}    // namespace CombatAndroid::ECS
