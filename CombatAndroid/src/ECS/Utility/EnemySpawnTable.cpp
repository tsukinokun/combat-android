//-------------------------------------------------------------
//! @file    EnemySpawnTable.cpp
//! @brief   湧かせる敵の種類と出現比重を定義するテーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/EnemySpawnTable.hpp>

#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 抽選テーブル本体。
        //
        // ★ 敵を1種追加するときはここへ1行足すだけでよい ★
        //     { EnemyTypeId::Xxx, "Xxx", &MakeXxxConfig, 重み, 解禁秒数 },
        //
        // weightは相対値なので合計を100に揃える必要はない。
        // 「SmallZombieがBigZombieの4倍出る」という比だけが意味を持つ。
        // 実装途中の敵はweight=0で置いておけば、抽選には出ないがテーブル上に
        // 存在は残せる（後でweightを上げるだけで有効化できる）
        //-------------------------------------------------------------
        constexpr EnemySpawnTableEntry kSpawnTable[] = {
            {EnemyTypeId::SmallZombie, "SmallZombie", &MakeSmallZombieConfig, 80.0f, 0.0f},
            // BigZombieは体力150・攻撃も痛いので、序盤30秒は出さずに比重も低くする
            {EnemyTypeId::BigZombie, "BigZombie", &MakeBigZombieConfig, 20.0f, 30.0f},
            // Paladinは武器を持って湧き、撃破するとその武器を落とすレア敵。
            // 体力200・EXP60と報酬が大きいぶん比重をぐっと下げ、序盤60秒は出さない。
            // 比重5は「20体に1体くらい混ざる」狙い（SmallZombie80 + BigZombie20 + Paladin5）
            {EnemyTypeId::Paladin, "Paladin", &MakePaladinConfig, 5.0f, 60.0f},
        };

        // 種類を足したのにテーブルへ書き忘れる事故を防ぐ
        static_assert(std::size(kSpawnTable) == static_cast<size_t>(EnemyTypeId::Count),
                      "EnemyTypeId に種類を足したら kSpawnTable にも1行足すこと");
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 抽選テーブル全体を得る
    //-------------------------------------------------------------
    std::span<const EnemySpawnTableEntry> GetEnemySpawnTable() {
        return std::span<const EnemySpawnTableEntry>(kSpawnTable);
    }

    //-------------------------------------------------------------
    //! @brief 比重に従って敵の種類を1つ抽選する
    //-------------------------------------------------------------
    const EnemySpawnTableEntry* PickEnemyType(std::mt19937& rng, float elapsedSeconds) {
        //---------------------------------------------------------
        // 1周目：解禁済みエントリの比重を合計する
        //---------------------------------------------------------
        float totalWeight = 0.0f;
        for(const EnemySpawnTableEntry& entry : kSpawnTable) {
            if(entry.weight > 0.0f && elapsedSeconds >= entry.unlockTimeSeconds)
                totalWeight += entry.weight;
        }

        if(totalWeight <= 0.0f)
            return nullptr;

        //---------------------------------------------------------
        // 2周目：累積比重のどこに落ちたかで決める。
        // 最後まで0を下回らなかった場合（浮動小数の丸め誤差）に備え、
        // 解禁済みの最後のエントリをそのまま返す保険を持たせる
        //---------------------------------------------------------
        std::uniform_real_distribution<float> distribution(0.0f, totalWeight);
        float                                 pick = distribution(rng);

        const EnemySpawnTableEntry* fallback = nullptr;
        for(const EnemySpawnTableEntry& entry : kSpawnTable) {
            if(entry.weight <= 0.0f || elapsedSeconds < entry.unlockTimeSeconds)
                continue;

            fallback = &entry;

            pick -= entry.weight;
            if(pick <= 0.0f)
                return &entry;
        }

        return fallback;
    }
}    // namespace CombatAndroid::ECS
