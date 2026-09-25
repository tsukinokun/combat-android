//-------------------------------------------------------------
//! @file    WeaponTable.hpp
//! @brief   武器の識別子と、レベルごとの攻撃力を定義するテーブルの宣言
//! @author  山﨑愛
//! @note    値は Assets/Tables/WeaponLevels.json が持ち、初回参照で1度だけ読む（SkillTableと同じ流儀）。
//!          武器の実効ステータス（WeaponComponent::damage）は常にこのテーブルから導出し、加算では積み上げない
//-------------------------------------------------------------
#pragma once

#include <array>
#include <span>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct WeaponComponent;

    //-------------------------------------------------------------
    //! @enum  WeaponId
    //! @brief 武器の識別子
    //! @note  値をセーブデータ等へ書き出してはいないため、並べ替えても構わない。
    //!        追加する場合はCountの手前へ足し、WeaponTable.cppのkWeaponKeysに名前を1つ、
    //!        Assets/Tables/ の武器ごとのJSON（WeaponLevels / WeaponEvolution）と
    //!        Assets/Prefabs/Weapon/ のPrefabにも1項目ずつ足すこと
    //!        （名前の数は.cpp側のstatic_assertで種類数と照合している）
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
        float damage = 0.0f;    //!< そのレベルでの基礎ダメージ（WeaponComponent::damageへそのまま書き戻す）
    };

    //-------------------------------------------------------------
    //! @struct WeaponTableEntry
    //! @brief  武器1種類ぶんのエントリ
    //-------------------------------------------------------------
    struct WeaponTableEntry {
        WeaponId     id = WeaponId::Warhammer;    //!< 種類の識別子
        std::wstring displayName;                  //!< 取得ログ・デバッグHUDに出す名前

        //! そのレベルごとの効果。levels[0]が1回目の取得（Lv1）に対応する
        std::array<WeaponLevelEntry, kMaxWeaponLevel> levels{};
    };

    //-------------------------------------------------------------
    //! @brief  武器の種類からテーブルJSON上の名前を引く関数
    //! @param  id [in] 武器の種類
    //! @return enumと同じ綴りの名前（例: "Warhammer"）。範囲外なら先頭の名前
    //! @note   武器ごとの値を持つJSON（WeaponLevels / WeaponEvolution / PaladinWeaponAttacks）は全てこの名前をキーにする
    //-------------------------------------------------------------
    [[nodiscard]]
    const char* GetWeaponKey(WeaponId id);

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
    //! @param  weapon [in,out] 対象のコンポーネント。weaponId/level/evolvedを読み、damageを書く
    //! @note   levelが1未満・kMaxWeaponLevel超の場合は範囲内へ丸めて引く
    //-------------------------------------------------------------
    void RecalculateWeaponStats(WeaponComponent& weapon);
}    // namespace CombatAndroid::ECS
