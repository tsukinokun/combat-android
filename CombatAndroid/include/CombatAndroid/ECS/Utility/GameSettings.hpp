//-------------------------------------------------------------
//! @file    GameSettings.hpp
//! @brief   オプション画面で変えられる設定（感度・音量・画面揺れ）の保持と読み書きの宣言
//! @note    RunRecordと同じ「キー=値」のテキストで Saves/Settings.txt に保存する。
//!          値は最初に参照したときに一度だけ読み、以後はメモリ上の1つを全体で共有する
//-------------------------------------------------------------
#pragma once

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // 各設定の範囲と1段の幅（オプション画面のA/Dで1段ずつ動く）
    //-------------------------------------------------------------
    inline constexpr float kMouseSensitivityMin  = 0.25f;    //!< マウス感度の倍率の下限
    inline constexpr float kMouseSensitivityMax  = 2.0f;     //!< 同じく上限
    inline constexpr float kMouseSensitivityStep = 0.25f;    //!< 同じく1段の幅
    inline constexpr int   kVolumePercentStep    = 10;       //!< 音量（0〜100%）の1段の幅

    //-------------------------------------------------------------
    //! @struct GameSettings
    //! @brief  プレイヤーが変えられる設定一式
    //-------------------------------------------------------------
    struct GameSettings {
        float mouseSensitivityScale = 1.0f;    //!< マウス感度の倍率（TpsCameraComponent::mouseSensitivityに掛ける）
        int   bgmVolumePercent      = 100;     //!< BGMの音量（0〜100%。PlayBgmの音量に掛ける）
        int   seVolumePercent       = 100;     //!< 効果音の音量（0〜100%。全ての効果音の音量に掛ける）
        bool  screenShakeEnabled    = true;    //!< 被弾時の画面揺れを出すか
    };

    //-------------------------------------------------------------
    //! @brief  今の設定を得る（初回だけファイルから読む）
    //! @return 全体で共有している設定。書き換えたらSaveGameSettingsで保存すること
    //-------------------------------------------------------------
    [[nodiscard]]
    GameSettings& GetGameSettings();

    //-------------------------------------------------------------
    //! @brief  今の設定をファイルへ保存する
    //! @return 保存できたらtrue
    //-------------------------------------------------------------
    bool SaveGameSettings();

    //-------------------------------------------------------------
    //! @brief  BGMの音量の倍率（0〜1）
    //-------------------------------------------------------------
    [[nodiscard]]
    float GetBgmVolumeScale();

    //-------------------------------------------------------------
    //! @brief  効果音の音量の倍率（0〜1）
    //-------------------------------------------------------------
    [[nodiscard]]
    float GetSeVolumeScale();
}    // namespace CombatAndroid::ECS
