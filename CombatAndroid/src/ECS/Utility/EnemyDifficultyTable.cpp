//-------------------------------------------------------------
//! @file    EnemyDifficultyTable.cpp
//! @brief   経過時間で敵を強化する「危険度ランク」テーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/EnemyDifficultyTable.hpp>

#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 危険度テーブル本体。
        //
        // ★ 難易度の伸び方を調整したいときはここの数値だけを触ればよい ★
        //     { 解禁秒数, HP倍率, EXP倍率, 攻撃力倍率, ひるみ閾値倍率 },
        //
        // 1分で1段（kDangerRankIntervalSeconds）。終端より先はkContinuationを
        // 1段ぶんずつ足し続けるため、曲線は頭打ちにならない。
        //
        // 数値の狙い：プレイヤーの与ダメージは走行中に約3.2倍まで伸びる
        // （ウォーハンマー38→70で1.84倍 × 憤怒Lv5の1.75倍。コンボ段の倍率は
        // 　どのレベルでも同じ係数なので打ち消し合う）。HPはそのわずかに先を行かせ、
        // 　育ちきったプレイヤーなら危険度10でもSmallZombieを軽2発で倒せる程度に留める。
        // 　危険度10の圧力の主役は湧き間隔の詰め（3.0秒→0.6秒＝レート5倍）であって、
        // 　HPが全部を背負う必要はない。
        //
        // 攻撃側を中盤以降で上向きにしてあるのは、スキルが7種（七つの大罪）へ増えて
        // 　プレイヤー側に防御・回復の手段が加わったため。傲慢（被ダメージ-30%）を伸ばすと
        // 　実効HPが1.43倍になり、以前の攻撃倍率のままでは危険度10でも被ダメージが
        // 　初期とほぼ変わらなくなってしまう。ここで相殺しておく。
        // 　一方でHP側は据え置きでよい ── 与ダメージの伸びしろは3.2倍のまま変わっておらず、
        // 　むしろ怠惰を取れば下がるため。
        // 　なお7種から3枚の抽選である以上、傲慢を引けない周回もある。取れなかった場合に
        // 　一方的な難化にならないよう、相殺は控えめ（危険度10で1.64→1.85）に留めてある
        //-------------------------------------------------------------
        constexpr EnemyDifficultyEntry kDifficultyTable[] = {
            //  解禁秒   HP     EXP    攻撃   ひるみ
            {  0.0f, 1.00f, 1.00f, 1.00f, 1.00f},    // 危険度1：素の値
            { 60.0f, 1.15f, 1.05f, 1.00f, 1.05f},    // 危険度2：武器もスキルもまだ育っていないので攻撃力は据え置く
            {120.0f, 1.35f, 1.12f, 1.08f, 1.12f},    // 危険度3：スキル1〜2個目が乗る頃
            {180.0f, 1.60f, 1.20f, 1.18f, 1.20f},
            {240.0f, 1.90f, 1.30f, 1.30f, 1.28f},    // 危険度5：傲慢・怠惰が育ち始める頃から攻撃側を上向きにする
            {300.0f, 2.20f, 1.40f, 1.42f, 1.36f},    // 危険度6：湧き間隔の詰めきり（kIntervalRampSeconds=300）と同時
            {360.0f, 2.50f, 1.52f, 1.54f, 1.44f},
            {420.0f, 2.80f, 1.64f, 1.66f, 1.52f},
            {480.0f, 3.10f, 1.78f, 1.76f, 1.60f},
            {540.0f, 3.40f, 1.92f, 1.85f, 1.68f},    // 危険度10：プレイヤーの伸びしろ（約3.2倍）とほぼ釣り合う
        };

        //-------------------------------------------------------------
        // テーブル終端より先で、1段ごとに足し続ける増分。
        // 最終段の伸び幅をそのまま延長したもので、ここから先は
        // 「本来なら死んでいるはずの時間帯」として素直に手が付けられなくなっていく
        //-------------------------------------------------------------
        constexpr EnemyDifficultyEntry kContinuation{kDangerRankIntervalSeconds, 0.30f, 0.14f, 0.11f, 0.08f};

        //! テーブルの段数。GetDangerRank / GetEnemyDifficultyScale が外挿の起点に使う
        constexpr int kDifficultyRankCount = static_cast<int>(std::size(kDifficultyTable));

        static_assert(kDifficultyTable[0].unlockTimeSeconds == 0.0f, "最初の段は0秒から解禁されていなければならない");
        static_assert(kDifficultyTable[0].healthScale == 1.0f && kDifficultyTable[0].expScale == 1.0f
                          && kDifficultyTable[0].attackScale == 1.0f && kDifficultyTable[0].knockbackThresholdScale == 1.0f,
                      "最初の段はEnemySpawnerが定義した素の値（倍率1.0）でなければならない");

        //-------------------------------------------------------------
        //! @brief  テーブルの並びと倍率の関係をコンパイル時に検査する関数
        //! @return 健全ならtrue
        //-------------------------------------------------------------
        constexpr bool IsDifficultyTableSane() {
            for(int i = 1; i < kDifficultyRankCount; ++i) {
                // 解禁秒数は昇順。逆転するとGetDangerRankが段を飛ばす
                if(kDifficultyTable[i].unlockTimeSeconds <= kDifficultyTable[i - 1].unlockTimeSeconds)
                    return false;

                // 各倍率は下がらない（時間が経って弱くなることはない）
                if(kDifficultyTable[i].healthScale < kDifficultyTable[i - 1].healthScale)
                    return false;
                if(kDifficultyTable[i].expScale < kDifficultyTable[i - 1].expScale)
                    return false;
                if(kDifficultyTable[i].attackScale < kDifficultyTable[i - 1].attackScale)
                    return false;
                if(kDifficultyTable[i].knockbackThresholdScale < kDifficultyTable[i - 1].knockbackThresholdScale)
                    return false;

                //-----------------------------------------------------
                // ひるみ閾値がHPより速く伸びると、閾値がmaxHealthを追い越して
                // 「ひるむ一撃＝必ず致死」になりノックバックのモーションが再生されなくなる
                // （EnemySpawner.cppのMakeSmallZombieConfigのコメントにある不具合そのもの）
                //-----------------------------------------------------
                if(kDifficultyTable[i].knockbackThresholdScale > kDifficultyTable[i].healthScale)
                    return false;
            }
            return true;
        }

        static_assert(IsDifficultyTableSane(), "危険度テーブルの並び／倍率の関係が壊れている");
        static_assert(kContinuation.knockbackThresholdScale <= kContinuation.healthScale,
                      "終端より先でもひるみ閾値がHPを追い越してはならない");
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 経過秒数から現在の危険度ランクを求める
    //-------------------------------------------------------------
    int GetDangerRank(float elapsedSeconds) {
        if(elapsedSeconds <= 0.0f)
            return 1;

        //---------------------------------------------------------
        // テーブル終端より先：1段＝kDangerRankIntervalSecondsで数え続ける。
        // 表を伸ばさなくても走行時間に上限が生まれないようにするための分岐
        //---------------------------------------------------------
        const EnemyDifficultyEntry& last = kDifficultyTable[kDifficultyRankCount - 1];
        if(elapsedSeconds >= last.unlockTimeSeconds) {
            const float over = elapsedSeconds - last.unlockTimeSeconds;
            return kDifficultyRankCount + static_cast<int>(over / kDangerRankIntervalSeconds);
        }

        //---------------------------------------------------------
        // テーブル内：解禁済みの最後の段を採る。
        // 段数は高々十数なので線形走査で十分（PickEnemyTypeと同じ方針）
        //---------------------------------------------------------
        int rank = 1;
        for(int i = 0; i < kDifficultyRankCount; ++i) {
            if(elapsedSeconds >= kDifficultyTable[i].unlockTimeSeconds)
                rank = i + 1;
        }
        return rank;
    }

    //-------------------------------------------------------------
    //! @brief 危険度ランクに対応する倍率を得る
    //-------------------------------------------------------------
    EnemyDifficultyEntry GetEnemyDifficultyScale(int rank) {
        if(rank <= 1)
            return kDifficultyTable[0];

        if(rank <= kDifficultyRankCount)
            return kDifficultyTable[rank - 1];

        // 終端より先は最終段から線形に外挿する
        const EnemyDifficultyEntry& last = kDifficultyTable[kDifficultyRankCount - 1];
        const float                 over = static_cast<float>(rank - kDifficultyRankCount);

        return EnemyDifficultyEntry{
            last.unlockTimeSeconds + kContinuation.unlockTimeSeconds * over,
            last.healthScale + kContinuation.healthScale * over,
            last.expScale + kContinuation.expScale * over,
            last.attackScale + kContinuation.attackScale * over,
            last.knockbackThresholdScale + kContinuation.knockbackThresholdScale * over,
        };
    }

    //-------------------------------------------------------------
    //! @brief 危険度テーブル全体を得る
    //-------------------------------------------------------------
    std::span<const EnemyDifficultyEntry> GetEnemyDifficultyTable() {
        return std::span<const EnemyDifficultyEntry>(kDifficultyTable);
    }

    //-------------------------------------------------------------
    //! @brief 生成パラメータを危険度ランクぶん底上げする
    //-------------------------------------------------------------
    void ApplyEnemyDifficulty(EnemySpawnConfig& config, int rank) {
        const EnemyDifficultyEntry scale = GetEnemyDifficultyScale(rank);

        config.maxHealth *= scale.healthScale;
        config.expReward *= scale.expScale;
        config.hitboxDamage *= scale.attackScale;

        //---------------------------------------------------------
        // ひるみ閾値はHPと必ず連動させる。据え置くと、伸びたプレイヤーの
        // 与ダメージが常に閾値を上回り、敵が触れるたびにひるんでAttackへ
        // 到達しなくなる。上げたHPが実質クラウドコントロールとして返金され、
        // 難易度カーブが一番効かせたい所で平らになってしまう
        //---------------------------------------------------------
        config.knockbackDamageThreshold *= scale.knockbackThresholdScale;
    }
}    // namespace CombatAndroid::ECS
