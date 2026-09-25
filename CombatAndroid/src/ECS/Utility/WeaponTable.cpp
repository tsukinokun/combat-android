//-------------------------------------------------------------
//! @file    WeaponTable.cpp
//! @brief   武器テーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/WeaponEvolutionTable.hpp>

#include <cereal/types/vector.hpp>

#include <algorithm>
#include <iterator>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // WeaponTableEntryのcerealシリアライズ定義。
    // 段ごとの基礎ダメージは "damage": [Lv1, Lv2, ...] の配列で持つ（段の数はkMaxWeaponLevelと一致させる）
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const WeaponTableEntry& entry) {
        std::vector<float> damage;
        for(const WeaponLevelEntry& level : entry.levels)
            damage.push_back(level.damage);

        SaveWideField(archive, "displayName", entry.displayName);
        archive(cereal::make_nvp("damage", damage));
    }

    template <class Archive>
    void load(Archive& archive, WeaponTableEntry& entry) {
        LoadWideField(archive, "displayName", entry.displayName);

        std::vector<float> damage;
        LoadField(archive, "damage", damage);
        if(damage.size() != entry.levels.size())
            Tsukino::Core::Log::Error("WeaponLevels.json: damage of " + WideToUtf8(entry.displayName) + " must have "
                                      + std::to_string(kMaxWeaponLevel) + " levels");

        for(size_t i = 0; i < std::min(damage.size(), entry.levels.size()); ++i)
            entry.levels[i].damage = damage[i];
    }

    namespace {
        constexpr const char* kWeaponTableFile = "WeaponLevels.json";
        constexpr const char* kWeaponTableRoot = "WeaponLevels";

        //! JSON上の武器名（WeaponIdの並び順）
        constexpr const char* kWeaponKeys[] = {"Warhammer", "Greatsword", "Battleaxe"};

        // 種類を足したのに名前を書き忘れる事故を防ぐ
        static_assert(std::size(kWeaponKeys) == static_cast<size_t>(WeaponId::Count),
                      "WeaponId に種類を足したら kWeaponKeys にも1つ足すこと");

        //-------------------------------------------------------------
        //! @struct WeaponTableData
        //! @brief  武器テーブル全体（WeaponIdの並び順）
        //-------------------------------------------------------------
        struct WeaponTableData {
            std::array<WeaponTableEntry, static_cast<size_t>(WeaponId::Count)> entries{};
        };

        template <class Archive>
        void save(Archive& archive, const WeaponTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i)
                archive(cereal::make_nvp(kWeaponKeys[i], data.entries[i]));
        }

        template <class Archive>
        void load(Archive& archive, WeaponTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i) {
                LoadField(archive, kWeaponKeys[i], data.entries[i]);
                if(data.entries[i].displayName.empty())
                    Tsukino::Core::Log::Error(std::string("WeaponLevels.json: ") + kWeaponKeys[i] + " is missing");
            }
        }

        //-------------------------------------------------------------
        //! @brief  武器テーブルを Assets/Tables/WeaponLevels.json から読む関数
        //! @return WeaponIdの並び順のテーブル（読めなかった武器は攻撃力0のまま）
        //-------------------------------------------------------------
        WeaponTableData LoadWeaponTable() {
            WeaponTableData data;
            for(size_t i = 0; i < data.entries.size(); ++i)
                data.entries[i].id = static_cast<WeaponId>(i);

            (void)LoadTableJson(kWeaponTableFile, kWeaponTableRoot, data);
            return data;
        }

        //-------------------------------------------------------------
        //! @brief  武器テーブルを得る関数（初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全）
        //-------------------------------------------------------------
        const WeaponTableData& GetWeaponTableData() {
            static const WeaponTableData s_data = LoadWeaponTable();
            return s_data;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 武器の種類からテーブルJSON上の名前を引く
    //-------------------------------------------------------------
    const char* GetWeaponKey(WeaponId id) {
        int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(WeaponId::Count))
            index = 0;
        return kWeaponKeys[index];
    }

    //-------------------------------------------------------------
    //! @brief 武器テーブル全体を得る
    //-------------------------------------------------------------
    std::span<const WeaponTableEntry> GetWeaponTable() {
        return std::span<const WeaponTableEntry>(GetWeaponTableData().entries);
    }

    //-------------------------------------------------------------
    //! @brief 識別子からエントリを引く
    //-------------------------------------------------------------
    const WeaponTableEntry& GetWeaponEntry(WeaponId id) {
        return GetWeaponTableData().entries[static_cast<size_t>(id)];
    }

    //-------------------------------------------------------------
    //! @brief weaponId/levelから実効ステータスを再計算する
    //-------------------------------------------------------------
    void RecalculateWeaponStats(WeaponComponent& weapon) {
        const WeaponTableEntry& entry      = GetWeaponEntry(weapon.weaponId);
        const int               levelIndex = std::clamp(weapon.level, 1, kMaxWeaponLevel) - 1;

        weapon.damage = entry.levels[static_cast<size_t>(levelIndex)].damage;

        // 進化済みなら進化の倍率を掛ける（範囲・射程などはApplyWeaponEvolutionが一度だけ書き換える）
        if(weapon.evolved)
            weapon.damage *= GetWeaponEvolution(weapon.weaponId).damageScale;
    }
}    // namespace CombatAndroid::ECS
