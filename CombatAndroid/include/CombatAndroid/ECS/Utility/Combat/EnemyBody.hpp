//-------------------------------------------------------------
//! @file    EnemyBody.hpp
//! @brief   敵の体の中心を引く共通処理の宣言
//! @note    ボスはアニメーションで胴体が根元から大きく離れる（蜘蛛のダウン）ので、
//!          体を狙う処理は根元のTransformではなく、ここで引いた中心を使う
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  敵の体の中心（当たり判定のカプセルの中心）をワールド座標で得る
    //! @param  registry [in] ECSレジストリ
    //! @param  entity   [in] 対象。CollisionComponentが無ければ根元の位置を返す
    //! @note   胴体が根元から離れる敵は、エンジンのBoneFollowColliderComponent（Prefab）でカプセルを
    //!         胴体のボーンへ付いていかせているので、ダウン中のボスでも見た目の体を指す
    //-------------------------------------------------------------
    [[nodiscard]]
    hlslpp::float3 GetEnemyBodyCenter(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);
}    // namespace CombatAndroid::ECS
