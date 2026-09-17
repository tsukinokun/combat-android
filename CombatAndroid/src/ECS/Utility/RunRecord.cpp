//-------------------------------------------------------------
//! @file    RunRecord.cpp
//! @brief   プレイをまたいで残すベスト記録の読み書きの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/RunRecord.hpp>

#include <Tsukino/Core/Log.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 保存先のディレクトリとファイル
        constexpr const char* kSaveDirectory = "Saves";
        constexpr const char* kSaveFilePath  = "Saves/RunRecord.txt";

        //-------------------------------------------------------------
        // ファイルは「キー=値」を1行ずつ並べたテキスト。
        // 人が開いて読めることと、項目を足しても古いファイルがそのまま読めることを優先した
        //-------------------------------------------------------------
        constexpr const char* kKeySurvivalSeconds = "bestSurvivalSeconds";
        constexpr const char* kKeyKills           = "bestKills";
        constexpr const char* kKeyLevel           = "bestLevel";
        constexpr const char* kKeyClearCount      = "clearCount";
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 保存済みの記録を読む
    //-------------------------------------------------------------
    RunRecord LoadRunRecord() {
        RunRecord record;

        std::ifstream file(kSaveFilePath);
        if(!file)
            return record;    // まだ一度も遊んでいない

        std::string line;
        while(std::getline(file, line)) {
            const size_t separator = line.find('=');
            if(separator == std::string::npos)
                continue;

            const std::string key   = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);

            // 壊れた値は読み飛ばし、その項目だけ初期値のままにする
            try {
                if(key == kKeySurvivalSeconds)
                    record.bestSurvivalSeconds = std::stof(value);
                else if(key == kKeyKills)
                    record.bestKills = std::stoi(value);
                else if(key == kKeyLevel)
                    record.bestLevel = std::stoi(value);
                else if(key == kKeyClearCount)
                    record.clearCount = std::stoi(value);
            } catch(const std::exception&) {
                Tsukino::Core::Log::Warn("RunRecord: ignored a broken value for " + key);
            }
        }

        return record;
    }

    //-------------------------------------------------------------
    //! @brief 記録を保存する
    //-------------------------------------------------------------
    bool SaveRunRecord(const RunRecord& record) {
        std::error_code error;
        std::filesystem::create_directories(kSaveDirectory, error);
        if(error) {
            Tsukino::Core::Log::Error("RunRecord: failed to create the save directory: " + error.message());
            return false;
        }

        std::ofstream file(kSaveFilePath, std::ios::trunc);
        if(!file) {
            Tsukino::Core::Log::Error(std::string("RunRecord: failed to open ") + kSaveFilePath);
            return false;
        }

        file << kKeySurvivalSeconds << '=' << record.bestSurvivalSeconds << '\n';
        file << kKeyKills << '=' << record.bestKills << '\n';
        file << kKeyLevel << '=' << record.bestLevel << '\n';
        file << kKeyClearCount << '=' << record.clearCount << '\n';

        return static_cast<bool>(file);
    }

    //-------------------------------------------------------------
    //! @brief 1プレイの結果を記録へ反映する
    //-------------------------------------------------------------
    RunRecordUpdate ApplyRunResult(RunRecord& record, float survivalSeconds, int kills, int level, bool cleared) {
        RunRecordUpdate update;

        if(survivalSeconds > record.bestSurvivalSeconds) {
            record.bestSurvivalSeconds = survivalSeconds;
            update.survivalSeconds     = true;
        }
        if(kills > record.bestKills) {
            record.bestKills = kills;
            update.kills     = true;
        }
        if(level > record.bestLevel) {
            record.bestLevel = level;
            update.level     = true;
        }
        if(cleared)
            ++record.clearCount;

        return update;
    }
}    // namespace CombatAndroid::ECS
