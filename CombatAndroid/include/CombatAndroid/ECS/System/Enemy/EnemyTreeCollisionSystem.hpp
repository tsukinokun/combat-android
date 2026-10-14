//-------------------------------------------------------------
//! @file   EnemyTreeCollisionSystem.hpp
//! @brief  敵が木の幹をすり抜けないよう押し出すシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyTreeCollisionSystem
    //! @brief  敵の体（上から見た円）が木の幹の円と重なっていたら、幹の外へ押し出す。
    //!         幹の中心から外へ押し出すので、斜めから当たった敵は幹の周りを滑って回り込む。
    //!         真正面から当たると押し出しと前進が打ち消し合って張り付くので、押し出しの一部を
    //!         幹の接線方向（プレイヤーに近づく側）へも振り分けて回り込ませる。
    //! @note   敵は物理のキャラクターを使わずTransformを直接動かす（Kinematicのセンサー）ので、
    //!         物理の幹では止まらない。幹の位置はTreeFieldSystemがTreeObstacleContextへ書いたものを使う。
    //!         実行順は敵が動くEnemyBehaviorSystem（Movement）の直後、物理の同期と行列の焼き込みの前
    //!         （SystemPriority::EnemyObstacle）。すり抜けた位置は一度も画面に出ない
    //-------------------------------------------------------------
    class EnemyTreeCollisionSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
