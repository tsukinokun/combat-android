//-------------------------------------------------------------
//! @file   RunClockSystem.hpp
//! @brief  RunClockSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  RunClockSystem
    //! @brief  RunClockComponent（走行の経過時間と危険度ランク）を進める唯一のシステム。
    //!         ランクが上がったフレームに演出用のタイマーを立て、画面右の取得ログへ
    //!         GameLogEventを流す。
    //! @note   全システムの先頭（SystemPriority::RunClock = -5）で動かすこと。
    //!         EnemySpawnDirectorSystemが同じフレームでこの値を読んで敵を強化し、
    //!         PlayerHudSystemが同じ値を表示するため、ここが最初に進んでいないと
    //!         「画面に出ている危険度」と「実際に湧いた敵の危険度」が1フレームずれる
    //-------------------------------------------------------------
    class RunClockSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
