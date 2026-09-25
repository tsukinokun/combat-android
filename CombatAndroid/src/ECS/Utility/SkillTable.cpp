//-------------------------------------------------------------
//! @file    SkillTable.cpp
//! @brief   スキルテーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <cereal/types/vector.hpp>

#include <algorithm>
#include <iterator>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // SkillLevelEntry / SkillTableEntry のcerealシリアライズ定義。
    // 段は "levels": [Lv1, Lv2, ...] の配列で持つ（段の数はkMaxSkillLevelと一致させる）
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const SkillLevelEntry& level) {
        SaveWideField(archive, "description", level.description);
        archive(cereal::make_nvp("value", level.value), cereal::make_nvp("value2", level.value2));
    }

    template <class Archive>
    void load(Archive& archive, SkillLevelEntry& level) {
        LoadWideField(archive, "description", level.description);
        LoadField(archive, "value", level.value);
        LoadField(archive, "value2", level.value2);
    }

    template <class Archive>
    void save(Archive& archive, const SkillTableEntry& entry) {
        const std::vector<SkillLevelEntry> levels(entry.levels.begin(), entry.levels.end());

        SaveWideField(archive, "displayName", entry.displayName);
        archive(cereal::make_nvp("backgroundTexturePath", entry.backgroundTexturePath),
                cereal::make_nvp("iconTexturePath", entry.iconTexturePath),
                cereal::make_nvp("panelColor", entry.panelColor),
                cereal::make_nvp("levels", levels));
    }

    template <class Archive>
    void load(Archive& archive, SkillTableEntry& entry) {
        LoadWideField(archive, "displayName", entry.displayName);
        LoadField(archive, "backgroundTexturePath", entry.backgroundTexturePath);
        LoadField(archive, "iconTexturePath", entry.iconTexturePath);
        LoadField(archive, "panelColor", entry.panelColor);

        std::vector<SkillLevelEntry> levels;
        LoadField(archive, "levels", levels);
        if(levels.size() != entry.levels.size())
            Tsukino::Core::Log::Error("Skills.json: levels of " + WideToUtf8(entry.displayName) + " must have "
                                      + std::to_string(kMaxSkillLevel) + " levels");

        for(size_t i = 0; i < std::min(levels.size(), entry.levels.size()); ++i)
            entry.levels[i] = levels[i];
    }

    namespace {
        constexpr const char* kSkillTableFile = "Skills.json";
        constexpr const char* kSkillTableRoot = "Skills";

        //! JSON上のスキル名（SkillIdの並び順）
        constexpr const char* kSkillKeys[] = {"Greed", "Gluttony", "Wrath", "Pride", "Envy", "Lust", "Sloth"};

        // 種類を足したのに名前を書き忘れる事故を防ぐ
        static_assert(std::size(kSkillKeys) == static_cast<size_t>(SkillId::Count),
                      "SkillId に種類を足したら kSkillKeys にも1つ足すこと");

        //-------------------------------------------------------------
        //! @struct SkillTableData
        //! @brief  スキルテーブル全体（SkillIdの並び順）
        //-------------------------------------------------------------
        struct SkillTableData {
            std::array<SkillTableEntry, static_cast<size_t>(SkillId::Count)> entries{};
        };

        template <class Archive>
        void save(Archive& archive, const SkillTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i)
                archive(cereal::make_nvp(kSkillKeys[i], data.entries[i]));
        }

        template <class Archive>
        void load(Archive& archive, SkillTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i) {
                LoadField(archive, kSkillKeys[i], data.entries[i]);
                if(data.entries[i].displayName.empty())
                    Tsukino::Core::Log::Error(std::string("Skills.json: ") + kSkillKeys[i] + " is missing");
            }
        }

        //-------------------------------------------------------------
        //! @brief  スキルテーブルを Assets/Tables/Skills.json から読む関数
        //! @return SkillIdの並び順のテーブル
        //-------------------------------------------------------------
        SkillTableData LoadSkillTable() {
            SkillTableData data;
            for(size_t i = 0; i < data.entries.size(); ++i)
                data.entries[i].id = static_cast<SkillId>(i);

            (void)LoadTableJson(kSkillTableFile, kSkillTableRoot, data);
            return data;
        }

        //-------------------------------------------------------------
        //! @brief  スキルテーブルを得る関数（初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全）
        //-------------------------------------------------------------
        const SkillTableData& GetSkillTableData() {
            static const SkillTableData s_data = LoadSkillTable();
            return s_data;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief スキルの種類からテーブルJSON上の名前を引く
    //-------------------------------------------------------------
    const char* GetSkillKey(SkillId id) {
        int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(SkillId::Count))
            index = 0;
        return kSkillKeys[index];
    }

    //-------------------------------------------------------------
    //! @brief テーブルJSON上の名前からスキルの種類を引く
    //-------------------------------------------------------------
    bool FindSkillByKey(const std::string& key, SkillId& out) {
        for(size_t i = 0; i < std::size(kSkillKeys); ++i) {
            if(key == kSkillKeys[i]) {
                out = static_cast<SkillId>(i);
                return true;
            }
        }
        return false;
    }

    //-------------------------------------------------------------
    //! @brief スキルテーブル全体を得る
    //-------------------------------------------------------------
    std::span<const SkillTableEntry> GetSkillTable() {
        return std::span<const SkillTableEntry>(GetSkillTableData().entries);
    }

    //-------------------------------------------------------------
    //! @brief 識別子からエントリを引く
    //-------------------------------------------------------------
    const SkillTableEntry& GetSkillEntry(SkillId id) {
        return GetSkillTableData().entries[static_cast<size_t>(id)];
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

        for(const SkillTableEntry& entry : GetSkillTableData().entries) {
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

        for(const SkillTableEntry& entry : GetSkillTableData().entries) {
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
