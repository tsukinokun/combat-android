//-------------------------------------------------------------
//! @file   HitStopComponent.hpp
//! @brief  HitStopComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct HitStopComponent
    //! @brief  ヒットストップの実行時状態。CombatSystemが「ヒットに関与した
    //!         プレイヤー/敵エンティティ」だけへ個別に付与し、HitStopSystemが
    //!         毎フレーム消費する（画面全体ではなく該当エンティティのみを止めるため）
    //-------------------------------------------------------------
    struct HitStopComponent {
        float remainingTime  = 0.0f;    //!< 残り時間（実時間・秒）。0より大きい間だけ有効
        float scale          = 1.0f;    //!< remainingTime > 0 の間、このエンティティの
                                        //!< アニメーション速度・移動入力へ掛けるスケール値
        float baseAnimSpeed  = 1.0f;    //!< ヒットストップ開始時点のplayback_speed。
                                        //!< playback_speedはコンボ段/回避の切り替わり時にしか
                                        //!< 書き換わらないため、ここへ毎フレーム掛け算すると
                                        //!< 複数フレーム分乗算されて0近くまで潰れたまま戻らなくなる。
                                        //!< 必ずこの基準値へ毎フレーム掛け直し、終了時にも
                                        //!< この値へ復元する
    };
}    // namespace CombatAndroid::ECS
