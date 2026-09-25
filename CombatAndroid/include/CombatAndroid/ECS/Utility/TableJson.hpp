//-------------------------------------------------------------
//! @file   TableJson.hpp
//! @brief  調整値テーブル（Assets/Tables/*.json）の読み書きの共通処理
//! @note   テーブルは起動後の初回参照で1度だけ読む（各Get*Tableの関数内static）。
//!         書式は「ルートキー → 種類名（enumの名前）→ 値」。種類名の並びは各テーブルの.cppが持つ
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/Log.hpp>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>

#include <filesystem>
#include <fstream>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! テーブルJSONの置き場所
    inline constexpr const char* kTableRoot = "CombatAndroid/Assets/Tables/";

    //-------------------------------------------------------------
    //! @brief  テーブルJSONを1本読む関数
    //! @param  fileName [in]  kTableRootからのファイル名（例: "Skills.json"）
    //! @param  rootKey  [in]  JSONのルートキー
    //! @param  out      [out] 読み込み先。読めなかったときは既定値のまま
    //! @return 読めたか
    //! @note   ファイルが無い・壊れているときはLog::Errorを出して続行する（落とさない）
    //-------------------------------------------------------------
    template <class T>
    bool LoadTableJson(const char* fileName, const char* rootKey, T& out) {
        const std::string path = std::string(kTableRoot) + fileName;

        std::ifstream is(path);
        if(!is.is_open()) {
            Tsukino::Core::Log::Error("Table not found: " + path);
            return false;
        }

        try {
            cereal::JSONInputArchive archive(is);
            archive(cereal::make_nvp(rootKey, out));
        } catch(const cereal::Exception& exception) {
            Tsukino::Core::Log::Error("Table is broken: " + path + " (" + exception.what() + ")");
            return false;
        }
        return true;
    }

    //-------------------------------------------------------------
    //! @brief  テーブルをJSONへ書き出す関数
    //! @param  path    [in] 書き出し先のパス（ディレクトリは無ければ作る）
    //! @param  rootKey [in] JSONのルートキー
    //! @param  data    [in] 書き出す値
    //! @note   ゲーム中には使わない。値を調べるとき（Logs/配下へ書き出して読んだ値と比べる等）の道具
    //-------------------------------------------------------------
    template <class T>
    void SaveTableJson(const std::string& path, const char* rootKey, const T& data) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());

        std::ofstream os(path);
        if(!os.is_open()) {
            Tsukino::Core::Log::Error("Failed to write table: " + path);
            return;
        }

        cereal::JSONOutputArchive archive(os);
        archive(cereal::make_nvp(rootKey, data));
    }
}    // namespace CombatAndroid::ECS
