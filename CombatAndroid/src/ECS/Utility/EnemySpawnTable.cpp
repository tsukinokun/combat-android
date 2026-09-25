//-------------------------------------------------------------
//! @file    EnemySpawnTable.cpp
//! @brief   湧かせる敵の種類と出現比重を定義するテーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/EnemySpawnTable.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <array>
#include <iterator>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // EnemySpawnTableEntryのcerealシリアライズ定義（JSONが持つのは比重と解禁秒数だけ）
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const EnemySpawnTableEntry& entry) {
        archive(cereal::make_nvp("weight", entry.weight), cereal::make_nvp("unlockTimeSeconds", entry.unlockTimeSeconds));
    }

    template <class Archive>
    void load(Archive& archive, EnemySpawnTableEntry& entry) {
        LoadField(archive, "weight", entry.weight);
        LoadField(archive, "unlockTimeSeconds", entry.unlockTimeSeconds);
    }

    namespace {
        constexpr const char* kEnemySpawnFile = "EnemySpawn.json";
        constexpr const char* kEnemySpawnRoot = "EnemySpawn";

        //-------------------------------------------------------------
        //! @struct EnemyTypeDefinition
        //! @brief  敵の種類とJSON上の名前・生成関数の対応（コード側に残す部分）
        //-------------------------------------------------------------
        struct EnemyTypeDefinition {
            EnemyTypeId        id;
            const char*        key;
            EnemyConfigFactory makeConfig;
        };

        //-------------------------------------------------------------
        // ★ 敵を1種追加するときはここへ1行足し、EnemySpawn.jsonに比重と解禁秒数を書く ★
        //     { EnemyTypeId::Xxx, "Xxx", &MakeXxxConfig },
        //-------------------------------------------------------------
        constexpr EnemyTypeDefinition kEnemyTypes[] = {
            {EnemyTypeId::SmallZombie, "SmallZombie", &MakeSmallZombieConfig},
            {EnemyTypeId::BigZombie, "BigZombie", &MakeBigZombieConfig},
            {EnemyTypeId::Paladin, "Paladin", &MakePaladinConfig},
        };

        // 種類を足したのに対応表へ書き忘れる事故を防ぐ
        static_assert(std::size(kEnemyTypes) == static_cast<size_t>(EnemyTypeId::Count),
                      "EnemyTypeId に種類を足したら kEnemyTypes にも1行足すこと");

        //-------------------------------------------------------------
        //! @struct EnemySpawnTableData
        //! @brief  抽選テーブル全体（EnemyTypeIdの並び順）
        //-------------------------------------------------------------
        struct EnemySpawnTableData {
            std::array<EnemySpawnTableEntry, static_cast<size_t>(EnemyTypeId::Count)> entries{};
        };

        template <class Archive>
        void save(Archive& archive, const EnemySpawnTableData& data) {
            for(const EnemySpawnTableEntry& entry : data.entries)
                archive(cereal::make_nvp(entry.debugName, entry));
        }

        template <class Archive>
        void load(Archive& archive, EnemySpawnTableData& data) {
            for(EnemySpawnTableEntry& entry : data.entries)
                LoadField(archive, entry.debugName, entry);
        }

        //-------------------------------------------------------------
        //! @brief  コード側の対応表だけを埋めたテーブルを作る関数（比重は0＝抽選に出ない）
        //-------------------------------------------------------------
        EnemySpawnTableData MakeEnemySpawnTableSkeleton() {
            EnemySpawnTableData data;
            for(size_t i = 0; i < data.entries.size(); ++i) {
                data.entries[i].id         = kEnemyTypes[i].id;
                data.entries[i].debugName  = kEnemyTypes[i].key;
                data.entries[i].makeConfig = kEnemyTypes[i].makeConfig;
            }
            return data;
        }

        //-------------------------------------------------------------
        //! @brief  抽選テーブルを Assets/Tables/EnemySpawn.json から読む関数
        //! @return EnemyTypeIdの並び順のテーブル（JSONに無い敵は比重0＝抽選に出ない）
        //-------------------------------------------------------------
        EnemySpawnTableData LoadEnemySpawnTable() {
            EnemySpawnTableData data = MakeEnemySpawnTableSkeleton();
            (void)LoadTableJson(kEnemySpawnFile, kEnemySpawnRoot, data);
            return data;
        }

        //-------------------------------------------------------------
        //! @brief  抽選テーブルを得る関数（初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全）
        //-------------------------------------------------------------
        const EnemySpawnTableData& GetEnemySpawnTableData() {
            static const EnemySpawnTableData s_data = LoadEnemySpawnTable();
            return s_data;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 敵の種類からテーブルJSON上の名前を引く
    //-------------------------------------------------------------
    const char* GetEnemyTypeKey(EnemyTypeId id) {
        int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(EnemyTypeId::Count))
            index = 0;
        return kEnemyTypes[index].key;
    }

    //-------------------------------------------------------------
    //! @brief 抽選テーブル全体を得る
    //-------------------------------------------------------------
    std::span<const EnemySpawnTableEntry> GetEnemySpawnTable() {
        return std::span<const EnemySpawnTableEntry>(GetEnemySpawnTableData().entries);
    }

    //-------------------------------------------------------------
    //! @brief 比重に従って敵の種類を1つ抽選する
    //-------------------------------------------------------------
    const EnemySpawnTableEntry* PickEnemyType(std::mt19937& rng, float elapsedSeconds) {
        //---------------------------------------------------------
        // 1周目：解禁済みエントリの比重を合計する
        //---------------------------------------------------------
        float totalWeight = 0.0f;
        for(const EnemySpawnTableEntry& entry : GetEnemySpawnTableData().entries) {
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
        for(const EnemySpawnTableEntry& entry : GetEnemySpawnTableData().entries) {
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
