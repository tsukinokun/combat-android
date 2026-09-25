//-------------------------------------------------------------
//! @file    EnemyDifficultyTable.cpp
//! @brief   経過時間で敵を強化する「危険度ランク」テーブルの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/EnemyDifficultyTable.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <cereal/types/vector.hpp>

#include <iterator>
#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // EnemyDifficultyEntryのcerealシリアライズ定義
    //-------------------------------------------------------------
    template <class Archive>
    void save(Archive& archive, const EnemyDifficultyEntry& entry) {
        archive(cereal::make_nvp("unlockTimeSeconds", entry.unlockTimeSeconds),
                cereal::make_nvp("healthScale", entry.healthScale),
                cereal::make_nvp("expScale", entry.expScale),
                cereal::make_nvp("attackScale", entry.attackScale),
                cereal::make_nvp("knockbackThresholdScale", entry.knockbackThresholdScale));
    }

    template <class Archive>
    void load(Archive& archive, EnemyDifficultyEntry& entry) {
        LoadField(archive, "unlockTimeSeconds", entry.unlockTimeSeconds);
        LoadField(archive, "healthScale", entry.healthScale);
        LoadField(archive, "expScale", entry.expScale);
        LoadField(archive, "attackScale", entry.attackScale);
        LoadField(archive, "knockbackThresholdScale", entry.knockbackThresholdScale);
    }

    namespace {
        constexpr const char* kDifficultyFile = "EnemyDifficulty.json";
        constexpr const char* kDifficultyRoot = "EnemyDifficulty";

        //-------------------------------------------------------------
        //! @struct EnemyDifficultyData
        //! @brief  危険度テーブル全体
        //! @note   段は1分で1段（rankIntervalSeconds）。終端より先はcontinuationを1段ぶんずつ
        //!         足し続けるため、曲線は頭打ちにならない。continuationの解禁秒数は段の間隔そのもの
        //-------------------------------------------------------------
        struct EnemyDifficultyData {
            float                             rankIntervalSeconds = 60.0f;    //!< 1段ぶんの秒数
            std::vector<EnemyDifficultyEntry> ranks{EnemyDifficultyEntry{}};  //!< 危険度1から順の段（最低1段）
            EnemyDifficultyEntry              continuation{0.0f, 0.0f, 0.0f, 0.0f, 0.0f};    //!< 終端より先の1段ぶんの増分
        };

        template <class Archive>
        void save(Archive& archive, const EnemyDifficultyData& data) {
            archive(cereal::make_nvp("rankIntervalSeconds", data.rankIntervalSeconds),
                    cereal::make_nvp("ranks", data.ranks),
                    cereal::make_nvp("continuation", data.continuation));
        }

        template <class Archive>
        void load(Archive& archive, EnemyDifficultyData& data) {
            LoadField(archive, "rankIntervalSeconds", data.rankIntervalSeconds);
            LoadField(archive, "ranks", data.ranks);
            LoadField(archive, "continuation", data.continuation);
        }

        //-------------------------------------------------------------
        //! @brief  テーブルの並びと倍率の関係を検査する関数
        //! @param  data [in] 検査するテーブル
        //! @note   壊れていてもゲームは続ける（Log::Errorで知らせるだけ）
        //-------------------------------------------------------------
        void ValidateDifficultyTable(const EnemyDifficultyData& data) {
            auto error = [](const std::string& message) { Tsukino::Core::Log::Error("EnemyDifficulty.json: " + message); };

            const EnemyDifficultyEntry& first = data.ranks.front();
            if(first.unlockTimeSeconds != 0.0f)
                error("the first rank must unlock at 0 seconds");

            // 最初の段はEnemySpawnerが定義した素の値（倍率1.0）でなければならない
            if(first.healthScale != 1.0f || first.expScale != 1.0f || first.attackScale != 1.0f || first.knockbackThresholdScale != 1.0f)
                error("the first rank must have all scales at 1.0");

            if(data.rankIntervalSeconds <= 0.0f)
                error("rankIntervalSeconds must be positive");

            for(size_t i = 1; i < data.ranks.size(); ++i) {
                const EnemyDifficultyEntry& previous = data.ranks[i - 1];
                const EnemyDifficultyEntry& current  = data.ranks[i];
                const std::string           rank     = "rank " + std::to_string(i + 1);

                // 解禁秒数は昇順。逆転するとGetDangerRankが段を飛ばす
                if(current.unlockTimeSeconds <= previous.unlockTimeSeconds)
                    error(rank + ": unlockTimeSeconds must increase");

                // 各倍率は下がらない（時間が経って弱くなることはない）
                if(current.healthScale < previous.healthScale || current.expScale < previous.expScale
                   || current.attackScale < previous.attackScale || current.knockbackThresholdScale < previous.knockbackThresholdScale)
                    error(rank + ": scales must not decrease");

                //-----------------------------------------------------
                // ひるみ閾値がHPより速く伸びると、閾値がmaxHealthを追い越して
                // 「ひるむ一撃＝必ず致死」になりノックバックのモーションが再生されなくなる
                //-----------------------------------------------------
                if(current.knockbackThresholdScale > current.healthScale)
                    error(rank + ": knockbackThresholdScale must not exceed healthScale");
            }

            if(data.continuation.knockbackThresholdScale > data.continuation.healthScale)
                error("continuation: knockbackThresholdScale must not exceed healthScale");
        }

        //-------------------------------------------------------------
        //! @brief  危険度テーブルを Assets/Tables/EnemyDifficulty.json から読む関数
        //! @return 読んだテーブル（読めなければ「危険度1だけ・倍率1.0」のまま）
        //-------------------------------------------------------------
        EnemyDifficultyData LoadDifficultyTable() {
            EnemyDifficultyData data;
            (void)LoadTableJson(kDifficultyFile, kDifficultyRoot, data);

            if(data.ranks.empty()) {
                Tsukino::Core::Log::Error("EnemyDifficulty.json: ranks is empty");
                data.ranks.push_back(EnemyDifficultyEntry{});
            }

            data.continuation.unlockTimeSeconds = data.rankIntervalSeconds;
            ValidateDifficultyTable(data);
            return data;
        }

        //-------------------------------------------------------------
        //! @brief  危険度テーブルを得る関数（初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全）
        //-------------------------------------------------------------
        const EnemyDifficultyData& GetDifficultyData() {
            static const EnemyDifficultyData s_data = LoadDifficultyTable();
            return s_data;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 危険度ランク1段ぶんの秒数を得る
    //-------------------------------------------------------------
    float GetDangerRankIntervalSeconds() {
        return GetDifficultyData().rankIntervalSeconds;
    }

    //-------------------------------------------------------------
    //! @brief 経過秒数から現在の危険度ランクを求める
    //-------------------------------------------------------------
    int GetDangerRank(float elapsedSeconds) {
        if(elapsedSeconds <= 0.0f)
            return 1;

        const EnemyDifficultyData& data      = GetDifficultyData();
        const int                  rankCount = static_cast<int>(data.ranks.size());

        //---------------------------------------------------------
        // テーブル終端より先：1段＝rankIntervalSecondsで数え続ける。
        // 表を伸ばさなくても走行時間に上限が生まれないようにするための分岐
        //---------------------------------------------------------
        const EnemyDifficultyEntry& last = data.ranks.back();
        if(elapsedSeconds >= last.unlockTimeSeconds) {
            const float over = elapsedSeconds - last.unlockTimeSeconds;
            return rankCount + static_cast<int>(over / data.rankIntervalSeconds);
        }

        //---------------------------------------------------------
        // テーブル内：解禁済みの最後の段を採る。
        // 段数は高々十数なので線形走査で十分（PickEnemyTypeと同じ方針）
        //---------------------------------------------------------
        int rank = 1;
        for(int i = 0; i < rankCount; ++i) {
            if(elapsedSeconds >= data.ranks[static_cast<size_t>(i)].unlockTimeSeconds)
                rank = i + 1;
        }
        return rank;
    }

    //-------------------------------------------------------------
    //! @brief 危険度ランクに対応する倍率を得る
    //-------------------------------------------------------------
    EnemyDifficultyEntry GetEnemyDifficultyScale(int rank) {
        const EnemyDifficultyData& data      = GetDifficultyData();
        const int                  rankCount = static_cast<int>(data.ranks.size());

        if(rank <= 1)
            return data.ranks.front();

        if(rank <= rankCount)
            return data.ranks[static_cast<size_t>(rank - 1)];

        // 終端より先は最終段から線形に外挿する
        const EnemyDifficultyEntry& last         = data.ranks.back();
        const EnemyDifficultyEntry& continuation = data.continuation;
        const float                 over         = static_cast<float>(rank - rankCount);

        return EnemyDifficultyEntry{
            last.unlockTimeSeconds + continuation.unlockTimeSeconds * over,
            last.healthScale + continuation.healthScale * over,
            last.expScale + continuation.expScale * over,
            last.attackScale + continuation.attackScale * over,
            last.knockbackThresholdScale + continuation.knockbackThresholdScale * over,
        };
    }

    //-------------------------------------------------------------
    //! @brief 危険度テーブル全体を得る
    //-------------------------------------------------------------
    std::span<const EnemyDifficultyEntry> GetEnemyDifficultyTable() {
        return std::span<const EnemyDifficultyEntry>(GetDifficultyData().ranks);
    }

    //-------------------------------------------------------------
    //! @brief 生成パラメータを危険度ランクぶん底上げする
    //-------------------------------------------------------------
    void ApplyEnemyDifficulty(EnemySpawnConfig& config, int rank) {
        const EnemyDifficultyEntry scale = GetEnemyDifficultyScale(rank);

        config.healthScale *= scale.healthScale;
        config.expScale *= scale.expScale;
        config.attackScale *= scale.attackScale;

        //---------------------------------------------------------
        // ひるみ閾値はHPと必ず連動させる。据え置くと、伸びたプレイヤーの
        // 与ダメージが常に閾値を上回り、敵が触れるたびにひるんでAttackへ
        // 到達しなくなる。上げたHPが実質クラウドコントロールとして返金され、
        // 難易度カーブが一番効かせたい所で平らになってしまう
        //---------------------------------------------------------
        config.knockbackThresholdScale *= scale.knockbackThresholdScale;
    }
}    // namespace CombatAndroid::ECS
