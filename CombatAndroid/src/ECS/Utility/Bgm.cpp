//-------------------------------------------------------------
//! @file    Bgm.cpp
//! @brief   BGMの再生・停止の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Bgm.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Audio/AudioAsset.hpp>
#include <Tsukino/Audio/AudioManager.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Path.hpp>

#include <memory>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @brief  BGMのアセットを引く
        //! @param  context [in] エンジンコンテキスト
        //! @param  path    [in] .wavのパス
        //! @return 読めたアセット。ファイルが無い等で読めなければnullptr
        //-------------------------------------------------------------
        [[nodiscard]]
        std::shared_ptr<Tsukino::Asset::AudioAsset> FindBgmAsset(Tsukino::EngineIntegration::EngineContext& context, const char* path) {
            if(!context.assetManager || !context.audioManager || path == nullptr)
                return nullptr;

            Tsukino::Asset::AssetHandle handle = context.assetManager->Load(Tsukino::Core::Path(path));
            if(!handle.IsValid())
                return nullptr;

            return std::dynamic_pointer_cast<Tsukino::Asset::AudioAsset>(context.assetManager->Get(handle));
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief BGMをループ再生する
    //-------------------------------------------------------------
    void PlayBgm(Tsukino::EngineIntegration::EngineContext& context, const char* path, float volume) {
        std::shared_ptr<Tsukino::Asset::AudioAsset> asset = FindBgmAsset(context, path);
        if(!asset) {
            // 素材を置いていないだけなので、警告ではなく情報として残す
            Tsukino::Core::Log::Info(std::string("BGM is not available (skipped): ") + path);
            return;
        }

        // 同じBGMが二重に鳴らないよう、鳴っていれば止めてから鳴らし直す
        context.audioManager->Stop(*asset);
        context.audioManager->Play(*asset, true, volume);
    }

    //-------------------------------------------------------------
    //! @brief BGMを止める
    //-------------------------------------------------------------
    void StopBgm(Tsukino::EngineIntegration::EngineContext& context, const char* path) {
        std::shared_ptr<Tsukino::Asset::AudioAsset> asset = FindBgmAsset(context, path);
        if(!asset)
            return;

        context.audioManager->Stop(*asset);
    }
}    // namespace CombatAndroid::ECS
