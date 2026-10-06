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
    //! @note   currentExpがrequiredExpに達するとExpOrbSystemがレベルを進め、
    //!         超過分を繰り越したうえでSkillSelectComponent::pendingLevelUpsを積む
    //!         （スキル選択メニューはSkillSelectSystemが出す）
    //-------------------------------------------------------------
    struct PlayerExperienceComponent {
        int level      = 1;    //!< 現在レベル
        int currentExp = 0;    //!< 現在のEXP（レベルアップ時にrequiredExpを引いて繰り越す）

        //! 次のレベルに必要なEXP。計算式（100 + (level-1) * 50）はExpOrbSystem側に置いてあり、
        //! ここの初期値はその式にlevel=1を入れた値と一致させておくこと
        int requiredExp = 100;
    };
}    // namespace CombatAndroid::ECS
