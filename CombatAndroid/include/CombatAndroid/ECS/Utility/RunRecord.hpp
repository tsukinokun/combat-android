//-------------------------------------------------------------
//! @file    RunRecord.hpp
//! @brief   プレイをまたいで残すベスト記録の読み書きの宣言
//-------------------------------------------------------------
#pragma once

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct RunRecord
    //! @brief  ベスト記録。項目ごとに独立して一番良い値を持つ
    //!         （生存時間の最長と撃破数の最多が別のプレイで出ていてもよい）
    //-------------------------------------------------------------
    struct RunRecord {
        float bestSurvivalSeconds = 0.0f;    //!< 最長の生存時間（秒）
        int   bestKills           = 0;       //!< 最多の撃破数
        int   bestLevel           = 0;       //!< 最高の到達レベル
        int   clearCount          = 0;       //!< クリアした回数
    };

    //-------------------------------------------------------------
    //! @struct RunRecordUpdate
    //! @brief  1プレイの結果を記録へ反映したときに、どの項目を更新したか
    //-------------------------------------------------------------
    struct RunRecordUpdate {
        bool survivalSeconds = false;    //!< 生存時間を更新した
        bool kills           = false;    //!< 撃破数を更新した
        bool level           = false;    //!< 到達レベルを更新した

        //-------------------------------------------------------------
        //! @brief  どれか1つでも更新したか
        //! @return true: 更新した項目がある
        //-------------------------------------------------------------
        [[nodiscard]]
        bool Any() const { return survivalSeconds || kills || level; }
    };

    //-------------------------------------------------------------
    //! @brief  保存済みの記録を読む
    //! @return 読んだ記録。ファイルが無い・読めない項目は初期値のまま
    //! @note   保存先はカレントディレクトリ基準の Saves/RunRecord.txt
    //!         （ログの Logs/ と同じ扱い。Debugはリポジトリ直下、Releaseはexeの隣）
    //-------------------------------------------------------------
    [[nodiscard]]
    RunRecord LoadRunRecord();

    //-------------------------------------------------------------
    //! @brief  記録を保存する
    //! @param  record [in] 保存する記録
    //! @return true: 保存できた
    //! @note   Saves/ が無ければ作る。失敗してもゲームは続けられるので、ログを出すだけにする
    //-------------------------------------------------------------
    bool SaveRunRecord(const RunRecord& record);

    //-------------------------------------------------------------
    //! @brief  1プレイの結果を記録へ反映する
    //! @param  record          [in,out] 反映先の記録
    //! @param  survivalSeconds [in]     今回の生存時間（秒）
    //! @param  kills           [in]     今回の撃破数
    //! @param  level           [in]     今回の到達レベル
    //! @param  cleared         [in]     今回クリアしたか
    //! @return どの項目を更新したか
    //-------------------------------------------------------------
    RunRecordUpdate ApplyRunResult(RunRecord& record, float survivalSeconds, int kills, int level, bool cleared);
}    // namespace CombatAndroid::ECS
