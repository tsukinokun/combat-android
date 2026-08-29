//-------------------------------------------------------------
//! @file    WeaponTable.cpp
//! @brief   武器テーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>

#include <algorithm>
#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 段階ごとの基礎ダメージ。
        //
        // ★ 攻撃力の伸び幅を調整したいときはここの数値だけを触ればよい ★
        //
        // Lv1の値は元々CombatAndroidScene.cppにハードコードされていた初期値
        // （warhammer=38, greatsword=22）を踏襲している。Lv2以降は暫定の伸び値であり、
        // 実際のバランスは実機で確認しながら詰める前提。
        // 配列の長さはkMaxWeaponLevelと一致していなければならない（下のstatic_assert）
        //-------------------------------------------------------------

        constexpr WeaponLevelEntry kWarhammerLevels[] = {
            {38.0f},
            {46.0f},
            {54.0f},
            {62.0f},
            {70.0f},
        };

        constexpr WeaponLevelEntry kGreatswordLevels[] = {
            {22.0f},
            {28.0f},
            {34.0f},
            {40.0f},
            {46.0f},
        };

        static_assert(std::size(kWarhammerLevels) == static_cast<size_t>(kMaxWeaponLevel), "kWarhammerLevels の段階数を kMaxWeaponLevel に合わせること");
        static_assert(std::size(kGreatswordLevels) == static_cast<size_t>(kMaxWeaponLevel), "kGreatswordLevels の段階数を kMaxWeaponLevel に合わせること");

        //-------------------------------------------------------------
        // 武器テーブル本体。
        //
        // ★ 武器を1種追加するときはここへ1行足す ★
        //     { WeaponId::Xxx, L"名前", kXxxLevels },
        //   併せてWeaponIdへの追加と、対応するkXxxLevelsの定義が要る。
        //
        // GetWeaponEntryがidを添字として使うため、必ずWeaponIdの並び順に定義すること
        //-------------------------------------------------------------
        constexpr WeaponTableEntry kWeaponTable[] = {
            {WeaponId::Warhammer, L"ウォーハンマー", kWarhammerLevels},
            {WeaponId::Greatsword, L"グレートソード", kGreatswordLevels},
        };

        // 種類を足したのにテーブルへ書き忘れる事故を防ぐ
        static_assert(std::size(kWeaponTable) == static_cast<size_t>(WeaponId::Count),
                      "WeaponId に種類を足したら kWeaponTable にも1行足すこと");
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 武器テーブル全体を得る
    //-------------------------------------------------------------
    std::span<const WeaponTableEntry> GetWeaponTable() {
        return std::span<const WeaponTableEntry>(kWeaponTable);
    }

    //-------------------------------------------------------------
    //! @brief 識別子からエントリを引く
    //-------------------------------------------------------------
    const WeaponTableEntry& GetWeaponEntry(WeaponId id) {
        return kWeaponTable[static_cast<size_t>(id)];
    }

    //-------------------------------------------------------------
    //! @brief weaponId/levelから実効ステータスを再計算する
    //-------------------------------------------------------------
    void RecalculateWeaponStats(WeaponComponent& weapon) {
        const WeaponTableEntry& entry      = GetWeaponEntry(weapon.weaponId);
        const int               levelIndex = std::clamp(weapon.level, 1, kMaxWeaponLevel) - 1;

        weapon.damage = entry.levels[static_cast<size_t>(levelIndex)].damage;
    }
}    // namespace CombatAndroid::ECS
