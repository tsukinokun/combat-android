//-------------------------------------------------------------
//! @file   BossComponent.hpp
//! @brief  BossComponent構造体の宣言
//-------------------------------------------------------------
#pragma once

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum  BossStage
    //! @brief どのボスか
    //-------------------------------------------------------------
    enum class BossStage : int {
        Mid = 0,    //!< 中ボス（走行の途中で1体）
        Final,      //!< ラスボス（倒すとクリア）
    };

    //-------------------------------------------------------------
    //! @struct BossComponent
    //! @brief  ボスであることを示す目印
    //! @note   中身は既存の敵（Prefab）を大きく硬くしたもの。湧き潰しの間引き
    //!         （SpawnedEnemyComponentを持つ個体が対象）には掛からないよう、
    //!         BossDirectorSystemはSpawnedEnemyComponentを付けずに生成する
    //-------------------------------------------------------------
    struct BossComponent {
        BossStage stage = BossStage::Mid;
    };
}    // namespace CombatAndroid::ECS
