//-------------------------------------------------------------
//! @file   GameLogEvent.hpp
//! @brief  画面右の取得ログへ1行流すための通知イベント
//-------------------------------------------------------------
#pragma once

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {

    //-------------------------------------------------------------
    //! @enum  GameLogCategory
    //! @brief 取得ログ1行の種別
    //! @note  追加する場合はCountの手前へ足し、GameLogSystem.cppのスタイル表にも
    //!        対応する1行を必ず足すこと（同ファイルのstatic_assertが種類数の一致を検査する）
    //-------------------------------------------------------------
    enum class GameLogCategory : int {
        WeaponAcquired = 0,    //!< 武器の新規取得
        WeaponLevelUp,         //!< 武器のレベルアップ（同じ種類を吸収した）
        PlayerLevelUp,         //!< プレイヤーのレベルアップ
        SkillAcquired,         //!< スキル取得
        Count,
    };

    //-------------------------------------------------------------
    //! @struct GameLogEvent
    //! @brief  「何が起きたか」だけを運ぶ通知イベント
    //! @note   発火側は種別と主題の文字列だけを渡す。ラベルの文言・色・レイアウト・
    //!         演出は全てGameLogSystemが持つ（SkillTable/WeaponTableが「何を選ばせるか」
    //!         だけを持ち、使い方を持たないのと同じ流儀）
    //-------------------------------------------------------------
    struct GameLogEvent {
        GameLogCategory category;    //!< 種別
        std::wstring    subject;     //!< 主題（武器名・スキル名・「Lv.3」など）
    };

}    // namespace CombatAndroid::ECS
