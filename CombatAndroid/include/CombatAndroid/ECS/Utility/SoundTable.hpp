//-------------------------------------------------------------
//! @file    SoundTable.hpp
//! @brief   効果音の種類と、鳴らし方（ファイル・音量・最短間隔）の表の宣言
//! @note    音源は Assets/Audio/generate_game_sounds.py がプロシージャル生成している。
//!          音を1つ足すときは、生成スクリプト・SoundId・SoundTable.cppのkSoundKeys・
//!          Assets/Tables/Sounds.json の4か所へ1つずつ足す
//-------------------------------------------------------------
#pragma once

#include <span>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum   SoundId
    //! @brief  効果音の種類。SoundTable.cppのkSoundKeys（Sounds.jsonのキー）と同じ並び順にすること
    //-------------------------------------------------------------
    enum class SoundId : int {
        PlayerHurt = 0,    //!< プレイヤーが被弾した
        EnemyDown,         //!< 敵を倒した
        Swing,             //!< 攻撃を振った
        Dodge,             //!< 回避した
        Pickup,            //!< 武器を拾った
        WeaponLevelUp,     //!< 武器のレベルが上がった
        LevelUp,           //!< プレイヤーのレベルが上がった
        SkillPick,         //!< スキルを取得した
        DangerUp,          //!< 危険度が上がった／ラスト1分に入った
        MenuMove,          //!< メニューのカーソルを動かした
        MenuConfirm,       //!< メニューで決定した
        RunClear,          //!< クリアした
        RunFailed,         //!< 倒された
        WeaponEvolve,      //!< 武器が進化した
        Count,
    };

    //-------------------------------------------------------------
    //! @struct SoundTableEntry
    //! @brief  効果音1種類ぶんの設定
    //! @note   値は Assets/Tables/Sounds.json が持つ（キーはSoundIdの名前）
    //-------------------------------------------------------------
    struct SoundTableEntry {
        SoundId     id          = SoundId::PlayerHurt;    //!< 種類
        std::string path;                                 //!< .wavのパス（空なら鳴らさない）
        float       volume      = 1.0f;                   //!< 再生音量（0〜1）
        float       minInterval = 0.0f;                   //!< 直前に同じ音を鳴らしてから、次に鳴らすまでの最短秒数
    };

    //-------------------------------------------------------------
    //! @brief  表全体を得る
    //! @return 表への参照（先読み等、全部を舐めたいとき用）
    //-------------------------------------------------------------
    [[nodiscard]]
    std::span<const SoundTableEntry> GetSoundTable();

    //-------------------------------------------------------------
    //! @brief  種類から設定を引く
    //! @param  id [in] 効果音の種類
    //! @return 対応する設定。範囲外なら先頭を返す
    //-------------------------------------------------------------
    [[nodiscard]]
    const SoundTableEntry& GetSoundEntry(SoundId id);
}    // namespace CombatAndroid::ECS
