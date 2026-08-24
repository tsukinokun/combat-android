//-------------------------------------------------------------
//! @file   PlayerExperienceComponent.hpp
//! @brief  PlayerExperienceComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerExperienceComponent
    //! @brief  プレイヤーの経験値・レベルを持つコンポーネント。PlayerHudSystemが
    //!         画面左上のEXPバー表示に、ExpOrbSystemが吸収時の加算に使う
    //! @note   levelとrequiredExpは将来のレベルアップ機能（MAXでレベルアップ→
    //!         スキル選択）のために用意してあるが、今回はcurrentExpをrequiredExpで
    //!         頭打ちにするだけで、レベルアップ処理そのものは実装しない
    //-------------------------------------------------------------
    struct PlayerExperienceComponent {
        int level       = 1;      //!< 現在レベル（将来のレベルアップ機能用。今回は表示のみ）
        int currentExp  = 0;      //!< 現在のEXP
        int requiredExp = 100;    //!< 次のレベルに必要なEXP（今回はこの値で頭打ちにする）
    };
}    // namespace CombatAndroid::ECS
