//-------------------------------------------------------------
//! @file   TreeColliderComponent.hpp
//! @brief  TreeColliderComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <cstdint>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct TreeColliderComponent
    //! @brief  木の幹の当たり判定に付ける目印
    //! @note   TreeFieldSystemがプレイヤーの近くの木に作る当たり判定のエンティティへ付ける。
    //!         武器や斬撃弾の当たり判定（物理のオーバーラップ）は木の幹も拾うので、これを見て
    //!         「木に当たった」と分かり、TreeHitEventで木を揺らす
    //-------------------------------------------------------------
    struct TreeColliderComponent {
        std::uint64_t cellKey = 0;    //!< 木が立っている升目のキー（TreeFieldSystemが木を見分けるのに使う）
    };
}    // namespace CombatAndroid::ECS
