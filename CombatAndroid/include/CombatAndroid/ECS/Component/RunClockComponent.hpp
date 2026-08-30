//-------------------------------------------------------------
//! @file   RunClockComponent.hpp
//! @brief  RunClockComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct RunClockComponent
    //! @brief  1回の走行（シーン開始から死亡まで）の経過時間と危険度をまとめて持つコンポーネント。
    //!         プレイヤーエンティティに付け、RunClockSystemだけが書き込む。
    //! @note   以前は同じ量の時計が2本あった。EnemySpawnDirectorSystem::m_elapsedSeconds
    //!         （死んでも止まらない）と PlayerHudComponent::survivalTime（死ぬと止まる）で、
    //!         死亡後にズレていく。HUDへ出す危険度と、実際に湧く敵へ適用される危険度が
    //!         食い違うのは許容できないため、ここへ1本化した。
    //!         セーブは存在せず、リトライはシーンの作り直しなので、既定値がそのまま
    //!         「走行のリセット」になる（PlayerSkillComponentと同じ割り切り）
    //-------------------------------------------------------------
    struct RunClockComponent {
        float elapsedSeconds   = 0.0f;    //!< 生存時間（秒）。プレイヤーが死ぬと加算を止める
        int   dangerRank       = 1;       //!< elapsedSecondsから決まる現在の危険度（1始まり）
        float rankUpFlashTimer = 0.0f;    //!< ランクが上がった瞬間に立てる演出用の残り時間（秒）
    };

    //! rankUpFlashTimerの初期値（秒）。RunClockSystemが立て、PlayerHudSystemが
    //! 残り時間の割合を演出の強さとして読むため、両者が同じ値を見る必要がある
    inline constexpr float kRankUpFlashDuration = 1.6f;
}    // namespace CombatAndroid::ECS
