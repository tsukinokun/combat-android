//-------------------------------------------------------------
//! @file    EnemySpawnTable.hpp
//! @brief   湧かせる敵の種類と出現比重を定義するテーブルの宣言
//! @author  山﨑愛
//! @note    「敵1体をどう組み立てるか」はEnemySpawnerの責務、「どの種類を
//!          どれくらいの割合で湧かせるか」はこちらの責務、と軸を分けている。
//!          同じファイルに混ぜると、敵を1種増やすたびに負荷試験
//!          （EnemySpawner.hppをincludeしている）まで再コンパイルになってしまう
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>

#include <hlsl++.h>

#include <random>
#include <span>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum  EnemyTypeId
    //! @brief 敵の種類の識別子
    //! @note  値をセーブデータ等へ書き出してはいないため、並べ替えても構わない。
    //!        追加する場合はCountの手前へ足し、EnemySpawnTable.cppのテーブルにも
    //!        対応する1行を必ず足すこと（足し忘れてもコンパイルは通ってしまうため、
    //!        .cpp側にstatic_assertで人数の一致を検査させている）
    //-------------------------------------------------------------
    enum class EnemyTypeId : int {
        SmallZombie = 0,
        BigZombie,
        Count,
    };

    //-------------------------------------------------------------
    //! @brief  EnemySpawnConfigを組み立てるファクトリ関数の型
    //! @note   MakeSmallZombieConfig / MakeBigZombieConfig と同じシグネチャ。
    //!         std::functionではなく生の関数ポインタにしているのは、テーブルを
    //!         constexpr（読み取り専用セクション）に置いて動的確保をゼロにするため
    //-------------------------------------------------------------
    using EnemyConfigFactory = EnemySpawnConfig (*)(Tsukino::EngineIntegration::EngineContext&, const hlslpp::float3&);

    //-------------------------------------------------------------
    //! @struct EnemySpawnTableEntry
    //! @brief  敵1種類ぶんの抽選エントリ
    //-------------------------------------------------------------
    struct EnemySpawnTableEntry {
        EnemyTypeId        id;                  //!< 種類の識別子
        const char*        debugName;           //!< ログ表示用の名前
        EnemyConfigFactory makeConfig;           //!< 生成パラメータを作る関数
        float              weight;               //!< 相対比重。0以下で抽選対象外（実装途中の敵を差し込んでおける）
        float              unlockTimeSeconds;    //!< 経過時間がこの値を超えるまで抽選対象に入らない（0で最初から）
    };

    //-------------------------------------------------------------
    //! @brief  抽選テーブル全体を得る関数
    //! @return テーブルへの読み取り専用ビュー
    //-------------------------------------------------------------
    [[nodiscard]]
    std::span<const EnemySpawnTableEntry> GetEnemySpawnTable();

    //-------------------------------------------------------------
    //! @brief  比重に従って敵の種類を1つ抽選する関数
    //! @param  rng            [in,out] 乱数生成器
    //! @param  elapsedSeconds [in]     ゲーム開始からの経過秒数（解禁判定に使う）
    //! @return 選ばれたエントリ。解禁済みかつweight>0のエントリが1つも無ければnullptr
    //! @note   累積比重のルーレット選択。エントリ数は高々十数なので線形走査で十分
    //-------------------------------------------------------------
    [[nodiscard]]
    const EnemySpawnTableEntry* PickEnemyType(std::mt19937& rng, float elapsedSeconds);
}    // namespace CombatAndroid::ECS
