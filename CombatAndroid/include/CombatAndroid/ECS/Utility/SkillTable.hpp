//-------------------------------------------------------------
//! @file    SkillTable.hpp
//! @brief   レベルアップ時に選べるスキルと、その段階ごとの効果を定義するテーブルの宣言
//! @author  山﨑愛
//! @note    EnemySpawnTableと同じ流儀：「何を選ばせるか」だけをここに集め、
//!          「選ばれた結果をどう使うか」はExpOrbSystem/CombatSystemの責務にしている。
//!          テーブルは静的な読み取り専用データとして持ち、動的確保は行わない
//-------------------------------------------------------------
#pragma once

#include <hlsl++.h>

#include <array>
#include <random>
#include <span>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum  SkillId
    //! @brief スキルの識別子
    //! @note  値をセーブデータ等へ書き出してはいないため、並べ替えても構わない。
    //!        追加する場合はCountの手前へ足し、SkillTable.cppのテーブルにも
    //!        対応する1行と、効果を反映するRecalculateSkillStatsのcase文を必ず足すこと
    //!        （足し忘れてもコンパイルは通ってしまうため、.cpp側にstatic_assertで
    //!        　種類数の一致を検査させている）
    //-------------------------------------------------------------
    enum class SkillId : int {
        Greed = 0,    //!< 強欲：ソウル（EXP玉）取得時の経験値量を増やす
        Gluttony,     //!< 暴食：ソウル取得時にHPを回復する
        Wrath,        //!< 憤怒：攻撃力が上がる
        Pride,        //!< 傲慢：被ダメージを軽減する
        Envy,         //!< 嫉妬：与えたダメージの一部をHPへ変換する
        Lust,         //!< 色欲：移動速度が上がる
        Sloth,        //!< 怠惰：常時HPが回復するが、その代わり攻撃力が下がる
        Count,
    };

    //! @brief 1つのスキルを取得できる回数の上限（＝テーブルに定義する段階数）
    inline constexpr int kMaxSkillLevel = 5;

    //! @brief レベルアップ時に一度に提示する選択肢の最大数
    inline constexpr int kSkillChoiceMax = 3;

    //-------------------------------------------------------------
    //! @struct SkillLevelEntry
    //! @brief  スキル1段階ぶんの効果
    //! @note   valueの意味はSkillIdごとに異なる：
    //!         Greed/Wrath/Pride/Lustは「加減する割合」（0.20で±20%）、
    //!         Gluttonyは「ソウル1個あたりに回復するHP」、
    //!         Envyは「与ダメージのうちHPへ変換する割合」、
    //!         Slothは「1秒あたりに回復するHP」。
    //!         解釈を持っているのはRecalculateSkillStats（SkillTable.cpp）だけ
    //-------------------------------------------------------------
    struct SkillLevelEntry {
        const wchar_t* description;    //!< カードに出す効果の説明文
        float          value;          //!< 効果量

        //! 2つ目の効果量。「上昇と引き換えに何かが下がる」スキルの、下がる側に使う。
        //! 既定値を持たせてあるので、1つしか効果を持たないスキルのテーブルは従来どおり
        //! 2要素で書けばよい（今これを使うのはSlothの攻撃力ペナルティだけ）
        float value2 = 0.0f;
    };

    //-------------------------------------------------------------
    //! @struct SkillTableEntry
    //! @brief  スキル1種類ぶんのエントリ
    //-------------------------------------------------------------
    struct SkillTableEntry {
        SkillId        id;                       //!< 種類の識別子
        const wchar_t* displayName;              //!< カードに出す名前
        const char*    backgroundTexturePath;    //!< カードの背景テクスチャ。専用の絵を用意したらここを差し替える
        hlslpp::float4 panelColor;               //!< 背景テクスチャに乗算する色（WhitePixel.pngを使っている間はこれが実質のカード色）

        //! そのスキルの段階ごとの効果。levels[0]が1回目の取得（Lv1）に対応する
        std::span<const SkillLevelEntry> levels;
    };

    //-------------------------------------------------------------
    //! @brief  スキルテーブル全体を得る関数
    //! @return テーブルへの読み取り専用ビュー
    //-------------------------------------------------------------
    [[nodiscard]]
    std::span<const SkillTableEntry> GetSkillTable();

    //-------------------------------------------------------------
    //! @brief  識別子からエントリを引く関数
    //! @param  id [in] 引きたいスキルの識別子
    //! @return 対応するエントリ（テーブルはidの並び順に定義されている）
    //-------------------------------------------------------------
    [[nodiscard]]
    const SkillTableEntry& GetSkillEntry(SkillId id);

    //-------------------------------------------------------------
    //! @brief  まだカンストしていないスキルから選択肢を重複なく抽選する関数
    //! @param  rng           [in,out] 乱数生成器
    //! @param  currentLevels [in]     現在の取得段階（PlayerSkillComponent::levels）
    //! @param  outCandidates [out]    抽選結果を先頭から詰める
    //! @return 実際に詰めた個数。0なら選べるスキルが1つも無い＝メニューを出さない
    //! @note   候補を集めてシャッフルし先頭から取る方式。棄却抽選と違って
    //!         「候補が3種に満たない場合は揃う分だけ」がそのまま自然に満たせる
    //-------------------------------------------------------------
    [[nodiscard]]
    int PickSkillCandidates(std::mt19937&                                 rng,
                            const std::array<int, static_cast<size_t>(SkillId::Count)>& currentLevels,
                            std::array<SkillId, kSkillChoiceMax>&         outCandidates);
}    // namespace CombatAndroid::ECS
