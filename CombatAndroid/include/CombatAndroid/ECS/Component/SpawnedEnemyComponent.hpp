//-------------------------------------------------------------
//! @file   SpawnedEnemyComponent.hpp
//! @brief  SpawnedEnemyComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct SpawnedEnemyComponent
    //! @brief  EnemySpawnDirectorSystemが湧かせた敵であることを示すタグ
    //! @note   遠くへ離れた個体の間引き対象を、この印の付いた個体だけに限定するために使う。
    //!         シーンが最初から置いている敵や負荷試験（EnemyStressTestSystem）が
    //!         湧かせた敵を巻き込んで消してしまわないための境界線
    //-------------------------------------------------------------
    struct SpawnedEnemyComponent {
        float aliveTime     = 0.0f;     //!< 湧いてからの経過秒数（フェードインの時間の進みに使う）
        float spawnDistance = 0.0f;     //!< 湧いた時点のプレイヤーとの水平距離（フェードインの距離の進みに使う）
        bool  fadeInDone    = false;    //!< 霧の中からのフェードインを終えたか（以後は不透明度に触らない）
    };
}    // namespace CombatAndroid::ECS
