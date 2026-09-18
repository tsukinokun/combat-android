//-------------------------------------------------------------
//! @file    SoundTable.cpp
//! @brief   効果音の種類と、鳴らし方（ファイル・音量・最短間隔）の表の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/SoundTable.hpp>

#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 表の本体。
        //
        // ★ 効果音を1種追加するときはここへ1行足すだけでよい ★
        //     { SoundId::Xxx, "パス", 音量, 最短間隔 },
        //   併せて SoundId への追加と、Assets/Audio/generate_game_sounds.py での生成が要る。
        //
        // 最短間隔は「同じ音が同じ瞬間に何本も重なって割れる」のを防ぐためのもの。
        // 群れを薙ぎ払うと撃破が一度に何体も出るので、そこだけ少し長めにしてある。
        // 音量は実機で聴いて決める値で、ヒット音（0.8）を基準に、
        // 頻繁に鳴るものほど控えめにしている
        //-------------------------------------------------------------
        constexpr SoundTableEntry kSoundTable[] = {
            {SoundId::PlayerHurt,    "CombatAndroid/Assets/Audio/PlayerHurt.wav",    0.85f, 0.05f},
            {SoundId::EnemyDown,     "CombatAndroid/Assets/Audio/EnemyDown.wav",     0.55f, 0.06f},
            {SoundId::Swing,         "CombatAndroid/Assets/Audio/Swing.wav",         0.40f, 0.05f},
            {SoundId::Dodge,         "CombatAndroid/Assets/Audio/Dodge.wav",         0.50f, 0.05f},
            {SoundId::Pickup,        "CombatAndroid/Assets/Audio/Pickup.wav",        0.70f, 0.00f},
            {SoundId::WeaponLevelUp, "CombatAndroid/Assets/Audio/WeaponLevelUp.wav", 0.70f, 0.00f},
            {SoundId::LevelUp,       "CombatAndroid/Assets/Audio/LevelUp.wav",       0.75f, 0.00f},
            {SoundId::SkillPick,     "CombatAndroid/Assets/Audio/SkillPick.wav",     0.70f, 0.00f},
            {SoundId::DangerUp,      "CombatAndroid/Assets/Audio/DangerUp.wav",      0.65f, 0.00f},
            {SoundId::MenuMove,      "CombatAndroid/Assets/Audio/MenuMove.wav",      0.45f, 0.00f},
            {SoundId::MenuConfirm,   "CombatAndroid/Assets/Audio/MenuConfirm.wav",   0.55f, 0.00f},
            {SoundId::RunClear,      "CombatAndroid/Assets/Audio/RunClear.wav",      0.80f, 0.00f},
            {SoundId::RunFailed,     "CombatAndroid/Assets/Audio/RunFailed.wav",     0.80f, 0.00f},
            {SoundId::WeaponEvolve,  "CombatAndroid/Assets/Audio/WeaponEvolve.wav",  0.80f, 0.00f},
        };

        // 種類を足したのに表へ書き忘れる事故を防ぐ（WeaponTable.cppと同じ作法）
        static_assert(std::size(kSoundTable) == static_cast<size_t>(SoundId::Count),
                      "SoundId に種類を足したら kSoundTable にも1行足すこと");
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 表全体を得る
    //-------------------------------------------------------------
    std::span<const SoundTableEntry> GetSoundTable() {
        return std::span<const SoundTableEntry>(kSoundTable);
    }

    //-------------------------------------------------------------
    //! @brief 種類から設定を引く
    //-------------------------------------------------------------
    const SoundTableEntry& GetSoundEntry(SoundId id) {
        int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(SoundId::Count))
            index = 0;

        return kSoundTable[index];
    }
}    // namespace CombatAndroid::ECS
