//-------------------------------------------------------------
//! @file    SoundTable.cpp
//! @brief   効果音の種類と、鳴らし方（ファイル・音量・最短間隔）の表の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/SoundTable.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <array>
#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // SoundTableEntryのcerealシリアライズ定義
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const SoundTableEntry& entry) {
        archive(cereal::make_nvp("path", entry.path),
                cereal::make_nvp("volume", entry.volume),
                cereal::make_nvp("minInterval", entry.minInterval));
    }

    template <class Archive>
    void load(Archive& archive, SoundTableEntry& entry) {
        LoadField(archive, "path", entry.path);
        LoadField(archive, "volume", entry.volume);
        LoadField(archive, "minInterval", entry.minInterval);
    }

    namespace {
        constexpr const char* kSoundTableFile = "Sounds.json";
        constexpr const char* kSoundTableRoot = "Sounds";

        //! JSON上の効果音名（SoundIdの並び順）
        constexpr const char* kSoundKeys[] = {
            "PlayerHurt", "EnemyDown", "Swing", "Dodge", "Pickup", "WeaponLevelUp", "LevelUp",
            "SkillPick", "DangerUp", "MenuMove", "MenuConfirm", "RunClear", "RunFailed", "WeaponEvolve",
        };

        // 種類を足したのに名前を書き忘れる事故を防ぐ
        static_assert(std::size(kSoundKeys) == static_cast<size_t>(SoundId::Count),
                      "SoundId に種類を足したら kSoundKeys にも1つ足すこと");

        //-------------------------------------------------------------
        //! @struct SoundTableData
        //! @brief  効果音の表全体（SoundIdの並び順）
        //-------------------------------------------------------------
        struct SoundTableData {
            std::array<SoundTableEntry, static_cast<size_t>(SoundId::Count)> entries{};
        };

        template <class Archive>
        void save(Archive& archive, const SoundTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i)
                archive(cereal::make_nvp(kSoundKeys[i], data.entries[i]));
        }

        template <class Archive>
        void load(Archive& archive, SoundTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i) {
                LoadField(archive, kSoundKeys[i], data.entries[i]);
                if(data.entries[i].path.empty())
                    Tsukino::Core::Log::Error(std::string("Sounds.json: ") + kSoundKeys[i] + " is missing");
            }
        }

        //-------------------------------------------------------------
        //! @brief  効果音の表を Assets/Tables/Sounds.json から読む関数
        //! @return SoundIdの並び順の表（JSONに無い音はパスが空＝鳴らない）
        //-------------------------------------------------------------
        SoundTableData LoadSoundTable() {
            SoundTableData data;
            for(size_t i = 0; i < data.entries.size(); ++i)
                data.entries[i].id = static_cast<SoundId>(i);

            (void)LoadTableJson(kSoundTableFile, kSoundTableRoot, data);
            return data;
        }

        //-------------------------------------------------------------
        //! @brief  効果音の表を得る関数（初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全）
        //-------------------------------------------------------------
        const SoundTableData& GetSoundTableData() {
            static const SoundTableData s_data = LoadSoundTable();
            return s_data;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 表全体を得る
    //-------------------------------------------------------------
    std::span<const SoundTableEntry> GetSoundTable() {
        return std::span<const SoundTableEntry>(GetSoundTableData().entries);
    }

    //-------------------------------------------------------------
    //! @brief 種類から設定を引く
    //-------------------------------------------------------------
    const SoundTableEntry& GetSoundEntry(SoundId id) {
        int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(SoundId::Count))
            index = 0;

        return GetSoundTableData().entries[static_cast<size_t>(index)];
    }
}    // namespace CombatAndroid::ECS
