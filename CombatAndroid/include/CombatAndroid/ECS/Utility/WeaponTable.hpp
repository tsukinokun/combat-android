//-------------------------------------------------------------
//! @file    WeaponTable.hpp
//! @brief   武器の識別子と、レベルごとの攻撃力を定義するテーブルの宣言
//! @author  山﨑愛
//! @note    SkillTableと同じ流儀：テーブルは静的な読み取り専用データとして持ち、
//!          動的確保は行わない。武器の実効ステータス（WeaponComponent::damage）は
//!          常にこのテーブルから導出し、加算では積み上げない
//-------------------------------------------------------------
#pragma once

#include <span>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct WeaponComponent;

    //-------------------------------------------------------------
    //! @enum  WeaponId
    //! @brief 武器の識別子
    //! @note  値をセーブデータ等へ書き出してはいないため、並べ替えても構わない。
    //!        追加する場合はCountの手前へ足し、WeaponTable.cppのテーブルにも
    //!        対応する1行（種類ぶんのkXxxLevels定義込み）を必ず足すこと
    //!        （足し忘れてもコンパイルは通ってしまうため、.cpp側にstatic_assertで
    //!        　種類数の一致を検査させている）
    //-------------------------------------------------------------
    enum class WeaponId : int {
        Warhammer = 0,
        Greatsword,
        Battleaxe,
        Count,
    };

    //! @brief 1つの武器がレベルアップできる回数の上限（＝テーブルに定義する段階数）
    inline constexpr int kMaxWeaponLevel = 5;

    //-------------------------------------------------------------
    //! @struct WeaponLevelEntry
    //! @brief  武器1段階ぶんの効果
    //! @note   SkillLevelEntryのvalue（加算割合）と違い、damageはそのレベルでの
    //!         基礎ダメージの実値そのもの。解釈を持っているのはRecalculateWeaponStats
    //!         （WeaponTable.cpp）だけ
    //-------------------------------------------------------------
    struct WeaponLevelEntry {
        float damage;    //!< そのレベルでの基礎ダメージ（WeaponComponent::damageへそのまま書き戻す）
    };

    //-------------------------------------------------------------
    //! @struct WeaponTableEntry
    //! @brief  武器1種類ぶんのエントリ
    //-------------------------------------------------------------
    struct WeaponTableEntry {
        WeaponId       id;             //!< 種類の識別子
        const wchar_t* displayName;    //!< デバッグHUDに出す名前

        //! そのレベルごとの効果。levels[0]が1回目の取得（Lv1）に対応する
        std::span<const WeaponLevelEntry> levels;
    };

    //-------------------------------------------------------------
    //! @brief  武器テーブル全体を得る関数
    //! @return テーブルへの読み取り専用ビュー
    //-------------------------------------------------------------
    [[nodiscard]]
    std::span<const WeaponTableEntry> GetWeaponTable();

    //-------------------------------------------------------------
    //! @brief  識別子からエントリを引く関数
    //! @param  id [in] 引きたい武器の識別子
    //! @return 対応するエントリ（テーブルはidの並び順に定義されている）
    //-------------------------------------------------------------
    [[nodiscard]]
    const WeaponTableEntry& GetWeaponEntry(WeaponId id);

    //-------------------------------------------------------------
    //! @brief  weaponId/levelからテーブルを引き、実効ステータス（damage）を書き戻す関数
    //! @param  weapon [in,out] 対象のコンポーネント。weaponId/levelを読み、damageを書く
    //! @note   levelが1未満・kMaxWeaponLevel超の場合は範囲内へ丸めて引く
    //-------------------------------------------------------------
    void RecalculateWeaponStats(WeaponComponent& weapon);
}    // namespace CombatAndroid::ECS
