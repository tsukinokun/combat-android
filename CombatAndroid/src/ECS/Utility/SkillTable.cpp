//-------------------------------------------------------------
//! @file    SkillTable.cpp
//! @brief   スキルテーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>

#include <algorithm>
#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 段階ごとの効果。
        //
        // ★ 効果量を調整したいときはここの数値だけを触ればよい ★
        //
        // descriptionはそのままカードへ出るので、数値を変えたら文言も合わせること
        // （テーブル外に数値を書いた箇所は無いので、ここが唯一の真実になる）。
        // 配列の長さはkMaxSkillLevelと一致していなければならない（下のstatic_assert）
        //-------------------------------------------------------------

        //! 強欲：ソウル取得時のEXPに (1.0 + value) を掛ける
        constexpr SkillLevelEntry kGreedLevels[] = {
            {L"ソウルから得るEXP +20%", 0.20f},
            {L"ソウルから得るEXP +40%", 0.40f},
            {L"ソウルから得るEXP +60%", 0.60f},
            {L"ソウルから得るEXP +80%", 0.80f},
            {L"ソウルから得るEXP +100%", 1.00f},
        };

        //! 暴食：ソウル1個につきvalueぶんHPを回復する（最大HPは超えない）
        constexpr SkillLevelEntry kGluttonyLevels[] = {
            {L"ソウル取得時 HPを3回復", 3.0f},
            {L"ソウル取得時 HPを6回復", 6.0f},
            {L"ソウル取得時 HPを9回復", 9.0f},
            {L"ソウル取得時 HPを12回復", 12.0f},
            {L"ソウル取得時 HPを15回復", 15.0f},
        };

        //! 憤怒：与ダメージに (1.0 + value) を掛ける
        constexpr SkillLevelEntry kWrathLevels[] = {
            {L"攻撃力 +15%", 0.15f},
            {L"攻撃力 +30%", 0.30f},
            {L"攻撃力 +45%", 0.45f},
            {L"攻撃力 +60%", 0.60f},
            {L"攻撃力 +75%", 0.75f},
        };

        //! 傲慢：被ダメージに (1.0 - value) を掛ける。
        //! 刻みを一定にせず先細りさせているのは、被ダメージ軽減が実効HPに対して非線形なため
        //! （-30%は実効HP1.43倍だが-50%なら2.0倍）。等間隔で伸ばすと終盤の1段だけが極端に効いてしまう
        constexpr SkillLevelEntry kPrideLevels[] = {
            {L"被ダメージ -8%", 0.08f},
            {L"被ダメージ -15%", 0.15f},
            {L"被ダメージ -21%", 0.21f},
            {L"被ダメージ -26%", 0.26f},
            {L"被ダメージ -30%", 0.30f},
        };

        //! 嫉妬：敵へ与えたダメージにvalueを掛けた分だけ自分のHPを回復する（最大HPは超えない）
        constexpr SkillLevelEntry kEnvyLevels[] = {
            {L"与ダメージの3%をHPに変換", 0.03f},
            {L"与ダメージの6%をHPに変換", 0.06f},
            {L"与ダメージの9%をHPに変換", 0.09f},
            {L"与ダメージの12%をHPに変換", 0.12f},
            {L"与ダメージの15%をHPに変換", 0.15f},
        };

        //! 色欲：移動速度に (1.0 + value) を掛ける
        constexpr SkillLevelEntry kLustLevels[] = {
            {L"移動速度 +8%", 0.08f},
            {L"移動速度 +16%", 0.16f},
            {L"移動速度 +24%", 0.24f},
            {L"移動速度 +32%", 0.32f},
            {L"移動速度 +40%", 0.40f},
        };

        //! 怠惰：毎秒valueぶんHPが回復する代わりに、攻撃力へ (1.0 - value2) が掛かる。
        //! 唯一の「諸刃」スキル。回復量は等間隔に伸ばし、ペナルティの伸びは先細りさせてある
        //! （深く取るほど1段あたりの損が減るので、中途半端に1枚だけ取るより伸ばしきる方が報われる）
        constexpr SkillLevelEntry kSlothLevels[] = {
            {L"毎秒 HP+0.6 / 攻撃力 -5%", 0.6f, 0.05f},
            {L"毎秒 HP+1.2 / 攻撃力 -8%", 1.2f, 0.08f},
            {L"毎秒 HP+1.8 / 攻撃力 -11%", 1.8f, 0.11f},
            {L"毎秒 HP+2.4 / 攻撃力 -14%", 2.4f, 0.14f},
            {L"毎秒 HP+3.0 / 攻撃力 -17%", 3.0f, 0.17f},
        };

        static_assert(std::size(kGreedLevels) == static_cast<size_t>(kMaxSkillLevel), "kGreedLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kGluttonyLevels) == static_cast<size_t>(kMaxSkillLevel), "kGluttonyLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kWrathLevels) == static_cast<size_t>(kMaxSkillLevel), "kWrathLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kPrideLevels) == static_cast<size_t>(kMaxSkillLevel), "kPrideLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kEnvyLevels) == static_cast<size_t>(kMaxSkillLevel), "kEnvyLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kLustLevels) == static_cast<size_t>(kMaxSkillLevel), "kLustLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kSlothLevels) == static_cast<size_t>(kMaxSkillLevel), "kSlothLevels の段階数を kMaxSkillLevel に合わせること");

        //-------------------------------------------------------------
        // スキルテーブル本体。
        //
        // ★ スキルを1種追加するときはここへ1行足す ★
        //     { SkillId::Xxx, L"名前", 背景テクスチャのパス, 色, kXxxLevels },
        //   併せてSkillIdへの追加と、RecalculateSkillStatsのcase文の追加が要る。
        //
        // backgroundTexturePathは今のところ全て白1色のWhitePixel.pngを指しており、
        // 実際の見た目はpanelColorの乗算で作っている。スキルごとの絵を用意したら
        // ここのパスを差し替えるだけで切り替わる（panelColorは白にすれば素の絵になる）。
        // GetSkillEntryがidを添字として使うため、必ずSkillIdの並び順に定義すること
        //-------------------------------------------------------------
        constexpr const char* kPlaceholderTexturePath = "CombatAndroid/Assets/Textures/UI/WhitePixel.png";

        // hlslpp::float4のコンストラクタはconstexprではない（SIMD型のため）ので、
        // 段階のテーブルと違いこちらはconstexprにできない。
        // 実体はどのみち静的な読み取り専用データで、動的確保は発生しない
        const SkillTableEntry kSkillTable[] = {
            {SkillId::Greed, L"強欲", kPlaceholderTexturePath, {0.85f, 0.72f, 0.20f, 1.0f}, kGreedLevels},
            {SkillId::Gluttony, L"暴食", kPlaceholderTexturePath, {0.35f, 0.70f, 0.35f, 1.0f}, kGluttonyLevels},
            {SkillId::Wrath, L"憤怒", kPlaceholderTexturePath, {0.80f, 0.25f, 0.25f, 1.0f}, kWrathLevels},
            {SkillId::Pride, L"傲慢", kPlaceholderTexturePath, {0.55f, 0.35f, 0.80f, 1.0f}, kPrideLevels},
            {SkillId::Envy, L"嫉妬", kPlaceholderTexturePath, {0.25f, 0.65f, 0.60f, 1.0f}, kEnvyLevels},
            {SkillId::Lust, L"色欲", kPlaceholderTexturePath, {0.90f, 0.40f, 0.65f, 1.0f}, kLustLevels},
            {SkillId::Sloth, L"怠惰", kPlaceholderTexturePath, {0.45f, 0.50f, 0.60f, 1.0f}, kSlothLevels},
        };

        // 種類を足したのにテーブルへ書き忘れる事故を防ぐ
        static_assert(std::size(kSkillTable) == static_cast<size_t>(SkillId::Count),
                      "SkillId に種類を足したら kSkillTable にも1行足すこと");
    }    // namespace

    //-------------------------------------------------------------
    //! @brief スキルテーブル全体を得る
    //-------------------------------------------------------------
    std::span<const SkillTableEntry> GetSkillTable() {
        return std::span<const SkillTableEntry>(kSkillTable);
    }

    //-------------------------------------------------------------
    //! @brief 識別子からエントリを引く
    //-------------------------------------------------------------
    const SkillTableEntry& GetSkillEntry(SkillId id) {
        return kSkillTable[static_cast<size_t>(id)];
    }

    //-------------------------------------------------------------
    //! @brief まだカンストしていないスキルから選択肢を重複なく抽選する
    //-------------------------------------------------------------
    int PickSkillCandidates(std::mt19937&                                              rng,
                            const std::array<int, static_cast<size_t>(SkillId::Count)>& currentLevels,
                            std::array<SkillId, kSkillChoiceMax>&                      outCandidates) {
        //---------------------------------------------------------
        // まだ伸ばせるスキルだけを集める
        //---------------------------------------------------------
        std::array<SkillId, static_cast<size_t>(SkillId::Count)> available{};
        int                                                       availableCount = 0;

        for(const SkillTableEntry& entry : kSkillTable) {
            if(currentLevels[static_cast<size_t>(entry.id)] >= kMaxSkillLevel)
                continue;    // カンスト済みは候補から外す
            available[static_cast<size_t>(availableCount)] = entry.id;
            ++availableCount;
        }

        if(availableCount <= 0)
            return 0;    // 全てカンスト。呼び出し側はメニューを出さずに進める

        //---------------------------------------------------------
        // シャッフルして先頭から必要数だけ取る。
        // std::shuffleは範囲全体を並べ替えるので、有効な範囲だけを渡す
        //---------------------------------------------------------
        std::shuffle(available.begin(), available.begin() + availableCount, rng);

        const int pickCount = std::min(availableCount, kSkillChoiceMax);
        for(int i = 0; i < pickCount; ++i)
            outCandidates[static_cast<size_t>(i)] = available[static_cast<size_t>(i)];

        return pickCount;
    }

    //-------------------------------------------------------------
    //! @brief levelsから実効値のキャッシュを計算し直す
    //-------------------------------------------------------------
    void RecalculateSkillStats(PlayerSkillComponent& skills) {
        // 一度「何も取っていない状態」へ戻してから積み直す
        skills.expGainMultiplier     = 1.0f;
        skills.healPerSoul           = 0.0f;
        skills.attackMultiplier      = 1.0f;
        skills.damageTakenMultiplier = 1.0f;
        skills.lifeStealRatio        = 0.0f;
        skills.moveSpeedMultiplier   = 1.0f;
        skills.healPerSecond         = 0.0f;

        for(const SkillTableEntry& entry : kSkillTable) {
            const int level = skills.levels[static_cast<size_t>(entry.id)];
            if(level <= 0)
                continue;    // 未取得

            // 段階は累積ではなく「その段階の値がそのまま今の効果」という定義なので、
            // levels[level-1]だけを見ればよい（Lv3を取ったらLv1とLv2の分は足さない）
            const SkillLevelEntry& levelEntry = entry.levels[static_cast<size_t>(std::min(level, kMaxSkillLevel) - 1)];
            const float            value      = levelEntry.value;

            switch(entry.id) {
            case SkillId::Greed:
                skills.expGainMultiplier = 1.0f + value;
                break;
            case SkillId::Gluttony:
                skills.healPerSoul = value;
                break;
            case SkillId::Wrath:
                // attackMultiplierは憤怒と怠惰の二者が書き込む唯一の値なので、代入ではなく乗算で積む。
                // リセット値が1.0なので、憤怒だけを取った場合の結果は代入していた頃と一致する
                skills.attackMultiplier *= 1.0f + value;
                break;
            case SkillId::Pride:
                skills.damageTakenMultiplier = 1.0f - value;
                break;
            case SkillId::Envy:
                skills.lifeStealRatio = value;
                break;
            case SkillId::Lust:
                skills.moveSpeedMultiplier = 1.0f + value;
                break;
            case SkillId::Sloth:
                skills.healPerSecond = value;
                skills.attackMultiplier *= 1.0f - levelEntry.value2;    // 回復と引き換えの攻撃力ペナルティ
                break;
            default:
                break;
            }
        }
    }
}    // namespace CombatAndroid::ECS
