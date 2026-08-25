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

        static_assert(std::size(kGreedLevels) == static_cast<size_t>(kMaxSkillLevel), "kGreedLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kGluttonyLevels) == static_cast<size_t>(kMaxSkillLevel), "kGluttonyLevels の段階数を kMaxSkillLevel に合わせること");
        static_assert(std::size(kWrathLevels) == static_cast<size_t>(kMaxSkillLevel), "kWrathLevels の段階数を kMaxSkillLevel に合わせること");

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
        skills.expGainMultiplier = 1.0f;
        skills.healPerSoul       = 0.0f;
        skills.attackMultiplier  = 1.0f;

        for(const SkillTableEntry& entry : kSkillTable) {
            const int level = skills.levels[static_cast<size_t>(entry.id)];
            if(level <= 0)
                continue;    // 未取得

            // 段階は累積ではなく「その段階の値がそのまま今の効果」という定義なので、
            // levels[level-1]だけを見ればよい（Lv3を取ったらLv1とLv2の分は足さない）
            const float value = entry.levels[static_cast<size_t>(std::min(level, kMaxSkillLevel) - 1)].value;

            switch(entry.id) {
            case SkillId::Greed:
                skills.expGainMultiplier = 1.0f + value;
                break;
            case SkillId::Gluttony:
                skills.healPerSoul = value;
                break;
            case SkillId::Wrath:
                skills.attackMultiplier = 1.0f + value;
                break;
            default:
                break;
            }
        }
    }
}    // namespace CombatAndroid::ECS
