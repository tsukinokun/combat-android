//-------------------------------------------------------------
//! @file    EnemyDifficultyTable.hpp
//! @brief   経過時間で敵を強化する「危険度ランク」テーブルの宣言
//! @author  山﨑愛
//! @note    「敵1体をどう組み立てるか」はEnemySpawner、「どの種類をどれくらいの
//!          割合で湧かせるか」はEnemySpawnTable、「時間が経つとどれだけ強くなるか」は
//!          こちらの責務、と軸を分けている。
//!          倍率を掛けるのはEnemySpawnDirectorSystemが湧かせた個体だけで、
//!          シーン手置きの敵とF1の負荷試験はここを通らず素の値のままになる
//!          （負荷試験は1体あたりのコストを一定に保つ必要があるため、これは意図した挙動）
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>

#include <span>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EnemyDifficultyEntry
    //! @brief  危険度ランク1段ぶんの倍率
    //! @note   hlslpp::float4はconstexprにできないため、表にはfloatだけを置いて
    //!         テーブル全体のconstexpr性を保っている（表示色はHUD側で計算する）
    //-------------------------------------------------------------
    struct EnemyDifficultyEntry {
        float unlockTimeSeconds;          //!< 経過時間がこの値以上でこの段になる（EnemySpawnTableEntryと同じ流儀）
        float healthScale;                //!< maxHealthに掛ける倍率
        float expScale;                   //!< expRewardに掛ける倍率
        float attackScale;                //!< hitboxDamageに掛ける倍率
        float knockbackThresholdScale;    //!< knockbackDamageThresholdに掛ける倍率
    };

    //! 危険度ランク1段ぶんの秒数。テーブル終端より先の外挿にも同じ刻みを使う
    inline constexpr float kDangerRankIntervalSeconds = 60.0f;

    //-------------------------------------------------------------
    //! @brief  経過秒数から現在の危険度ランクを求める関数
    //! @param  elapsedSeconds [in] シーン開始からの経過秒数
    //! @return 危険度ランク（1始まり）。上限は無く、テーブル終端より先も数え上がり続ける
    //-------------------------------------------------------------
    [[nodiscard]]
    int GetDangerRank(float elapsedSeconds);

    //-------------------------------------------------------------
    //! @brief  危険度ランクに対応する倍率を得る関数
    //! @param  rank [in] 危険度ランク（1始まり）
    //! @return 倍率一式。テーブルの段数を超えた場合は最終段からの外挿値を返す
    //-------------------------------------------------------------
    [[nodiscard]]
    EnemyDifficultyEntry GetEnemyDifficultyScale(int rank);

    //-------------------------------------------------------------
    //! @brief  危険度テーブル全体を得る関数
    //! @return テーブルへの読み取り専用ビュー
    //-------------------------------------------------------------
    [[nodiscard]]
    std::span<const EnemyDifficultyEntry> GetEnemyDifficultyTable();

    //-------------------------------------------------------------
    //! @brief  生成パラメータを危険度ランクぶん底上げする関数
    //! @param  config [in,out] 底上げする生成パラメータ
    //! @param  rank   [in]     危険度ランク（1始まり）
    //! @note   moveSpeed / attackRange / attackCooldown は意図的に触らない。
    //!         足の速さと間合いは「囲まれ方」そのものを変えてしまい、
    //!         EnemySpawnDirectorSystemの湧き間隔の詰めと効果が二重に乗るため
    //-------------------------------------------------------------
    void ApplyEnemyDifficulty(EnemySpawnConfig& config, int rank);
}    // namespace CombatAndroid::ECS
