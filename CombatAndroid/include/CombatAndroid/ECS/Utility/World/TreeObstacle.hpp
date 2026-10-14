//-------------------------------------------------------------
//! @file   TreeObstacle.hpp
//! @brief  木の幹の位置と太さ（敵が幹をすり抜けないための押し出しに使う）
//! @note   TreeFieldSystemが毎フレーム、描画範囲の木の幹をregistryのコンテキストへ書き、
//!         EnemyTreeCollisionSystemが敵の周りの幹を引いて重なりを押し出す。
//!         敵は物理のキャラクターを使わずTransformを直接動かす（Kinematicのセンサー）ので、
//!         物理の静的な幹（TreeFieldSystemの当たり判定）では止まらない。そのための2Dの円の情報
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>

#include <cmath>
#include <cstdint>
#include <unordered_map>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct TreeObstacle
    //! @brief  幹1本を上から見た円
    //-------------------------------------------------------------
    struct TreeObstacle {
        float x      = 0.0f;    //!< 幹の中心のX
        float z      = 0.0f;    //!< 幹の中心のZ
        float radius = 0.0f;    //!< 幹の半径（当たり判定のカプセルと同じ太さ）
    };

    //-------------------------------------------------------------
    //! @struct TreeObstacleContext
    //! @brief  描画範囲の幹の一覧（升目キー → 幹）。registryのコンテキストに置く
    //! @note   木は升目1つに1本までで、幹の中心は必ず自分の升目の中にある（TreeFieldSystem::ResolveTree）。
    //!         だから、ある位置の周りの幹は「その升目と周り8升目」を見れば全部見つかる
    //-------------------------------------------------------------
    struct TreeObstacleContext {
        float                                        cellSize = 700.0f;    //!< 升目の一辺（TreeFieldComponent::cellSize）
        std::unordered_map<std::uint64_t, TreeObstacle> trunks;            //!< 升目キー（MakeTreeCellKey）→ 幹
    };

    //-------------------------------------------------------------
    //! @brief  升目の座標を1つのキーにまとめる（TreeFieldSystemの升目キーと同じ作り方）
    //-------------------------------------------------------------
    [[nodiscard]]
    inline std::uint64_t MakeTreeCellKey(int cellX, int cellZ) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellX)) << 32) | static_cast<std::uint32_t>(cellZ);
    }

    //-------------------------------------------------------------
    //! @brief  位置(x, z)の周りの幹（その升目と周り8升目）を1本ずつ渡す
    //! @param  context [in] 幹の一覧
    //! @param  x       [in] 調べる位置のX
    //! @param  z       [in] 調べる位置のZ
    //! @param  fn      [in] 幹1本ごとに呼ぶ関数（const TreeObstacle&を受け取る）
    //! @note   升目の一辺より大きいものの当たりは調べられない（敵や幹の半径は升目よりずっと小さい前提）
    //-------------------------------------------------------------
    template <class Fn>
    void ForEachTreeObstacleNear(const TreeObstacleContext& context, float x, float z, Fn&& fn) {
        const float cellSize = context.cellSize > 0.0f ? context.cellSize : 700.0f;
        const int   cellX    = static_cast<int>(std::floor(x / cellSize));
        const int   cellZ    = static_cast<int>(std::floor(z / cellSize));
        for(int dz = -1; dz <= 1; ++dz) {
            for(int dx = -1; dx <= 1; ++dx) {
                const auto it = context.trunks.find(MakeTreeCellKey(cellX + dx, cellZ + dz));
                if(it != context.trunks.end())
                    fn(it->second);
            }
        }
    }
}    // namespace CombatAndroid::ECS
