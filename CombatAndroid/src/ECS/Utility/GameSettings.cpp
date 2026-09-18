//-------------------------------------------------------------
//! @file    GameSettings.cpp
//! @brief   オプション画面で変えられる設定の保持と読み書きの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/GameSettings.hpp>

#include <Tsukino/Core/Log.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 保存先のディレクトリとファイル（RunRecordと同じ場所）
        constexpr const char* kSaveDirectory = "Saves";
        constexpr const char* kSaveFilePath  = "Saves/Settings.txt";

        constexpr const char* kKeyMouseSensitivity = "mouseSensitivityScale";
        constexpr const char* kKeyBgmVolume        = "bgmVolumePercent";
        constexpr const char* kKeySeVolume         = "seVolumePercent";
        constexpr const char* kKeyScreenShake      = "screenShakeEnabled";

        //-------------------------------------------------------------
        //! @brief  ファイルから設定を読む
        //! @return 読んだ設定。ファイルが無い・壊れた項目は初期値のまま
        //! @note   手で書き換えられても範囲外にならないよう、読んだ後に丸める
        //-------------------------------------------------------------
        [[nodiscard]]
        GameSettings LoadGameSettings() {
            GameSettings settings;

            std::ifstream file(kSaveFilePath);
            if(!file)
                return settings;    // まだ一度も設定を変えていない

            std::string line;
            while(std::getline(file, line)) {
                const size_t separator = line.find('=');
                if(separator == std::string::npos)
                    continue;

                const std::string key   = line.substr(0, separator);
                const std::string value = line.substr(separator + 1);

                try {
                    if(key == kKeyMouseSensitivity)
                        settings.mouseSensitivityScale = std::stof(value);
                    else if(key == kKeyBgmVolume)
                        settings.bgmVolumePercent = std::stoi(value);
                    else if(key == kKeySeVolume)
                        settings.seVolumePercent = std::stoi(value);
                    else if(key == kKeyScreenShake)
                        settings.screenShakeEnabled = std::stoi(value) != 0;
                } catch(const std::exception&) {
                    Tsukino::Core::Log::Warn("GameSettings: ignored a broken value for " + key);
                }
            }

            settings.mouseSensitivityScale = std::clamp(settings.mouseSensitivityScale, kMouseSensitivityMin, kMouseSensitivityMax);
            settings.bgmVolumePercent      = std::clamp(settings.bgmVolumePercent, 0, 100);
            settings.seVolumePercent       = std::clamp(settings.seVolumePercent, 0, 100);

            return settings;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 今の設定を得る
    //-------------------------------------------------------------
    GameSettings& GetGameSettings() {
        static GameSettings s_settings = LoadGameSettings();
        return s_settings;
    }

    //-------------------------------------------------------------
    //! @brief 今の設定をファイルへ保存する
    //-------------------------------------------------------------
    bool SaveGameSettings() {
        const GameSettings& settings = GetGameSettings();

        std::error_code error;
        std::filesystem::create_directories(kSaveDirectory, error);
        if(error) {
            Tsukino::Core::Log::Error("GameSettings: failed to create the save directory: " + error.message());
            return false;
        }

        std::ofstream file(kSaveFilePath, std::ios::trunc);
        if(!file) {
            Tsukino::Core::Log::Error(std::string("GameSettings: failed to open ") + kSaveFilePath);
            return false;
        }

        file << kKeyMouseSensitivity << '=' << settings.mouseSensitivityScale << '\n';
        file << kKeyBgmVolume << '=' << settings.bgmVolumePercent << '\n';
        file << kKeySeVolume << '=' << settings.seVolumePercent << '\n';
        file << kKeyScreenShake << '=' << (settings.screenShakeEnabled ? 1 : 0) << '\n';

        return static_cast<bool>(file);
    }

    //-------------------------------------------------------------
    //! @brief BGMの音量の倍率
    //-------------------------------------------------------------
    float GetBgmVolumeScale() {
        return static_cast<float>(GetGameSettings().bgmVolumePercent) / 100.0f;
    }

    //-------------------------------------------------------------
    //! @brief 効果音の音量の倍率
    //-------------------------------------------------------------
    float GetSeVolumeScale() {
        return static_cast<float>(GetGameSettings().seVolumePercent) / 100.0f;
    }
}    // namespace CombatAndroid::ECS
