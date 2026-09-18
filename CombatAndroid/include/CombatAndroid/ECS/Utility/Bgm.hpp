//-------------------------------------------------------------
//! @file    Bgm.hpp
//! @brief   BGMの再生・停止の宣言
//! @note    BGMは合成せず、実素材の .wav を Assets/Audio へ置く運用にしている
//!          （詳細は Assets/Audio/README.md）。ファイルが無ければ何も鳴らないだけで、
//!          ゲームはそのまま動く
//-------------------------------------------------------------
#pragma once

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! タイトル画面のBGM。置かれていなければ無音
    inline constexpr const char* kTitleBgmPath = "CombatAndroid/Assets/Audio/BgmTitle.wav";
    //! 戦闘中のBGM。置かれていなければ無音
    inline constexpr const char* kBattleBgmPath = "CombatAndroid/Assets/Audio/BgmBattle.wav";

    //! BGMの音量。効果音（SoundTable）に埋もれず、かつ前に出過ぎない値
    inline constexpr float kBgmVolume = 0.35f;

    //-------------------------------------------------------------
    //! @brief  BGMをループ再生する
    //! @param  context [in] エンジンコンテキスト
    //! @param  path    [in] .wavのパス
    //! @param  volume  [in] 音量（オプションのBGM音量はここへさらに掛ける）
    //! @note   シーンのOnInitializeから呼ぶ。ファイルが無い等で読めなければ何もしない
    //-------------------------------------------------------------
    void PlayBgm(Tsukino::EngineIntegration::EngineContext& context, const char* path, float volume = kBgmVolume);

    //-------------------------------------------------------------
    //! @brief  鳴っているBGMを、今のオプションの音量で鳴らし直す
    //! @param  context [in] エンジンコンテキスト
    //! @note   エンジンには再生中の音量を変えるAPIが無いので、止めて最初から鳴らし直す。
    //!         オプション画面を閉じたときに呼ぶ（値を変えるたびに呼ぶと曲が頭に戻り続ける）
    //-------------------------------------------------------------
    void ReapplyBgmVolume(Tsukino::EngineIntegration::EngineContext& context);

    //-------------------------------------------------------------
    //! @brief  BGMを止める
    //! @param  context [in] エンジンコンテキスト
    //! @param  path    [in] 止める.wavのパス
    //! @note   シーンのOnExitから呼ぶ。止め忘れるとシーンを移っても鳴り続ける
    //-------------------------------------------------------------
    void StopBgm(Tsukino::EngineIntegration::EngineContext& context, const char* path);
}    // namespace CombatAndroid::ECS
