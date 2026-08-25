//-------------------------------------------------------------
//! @file   GameOverComponent.hpp
//! @brief  GameOverComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct GameOverComponent
    //! @brief  死亡演出からGAME OVER表示・リトライまでの進行状態。
    //!         プレイヤーエンティティに1つだけ付ける（PlayerHudComponentと同じ考え方）
    //-------------------------------------------------------------
    struct GameOverComponent {
        Tsukino::ECS::Entity titleTextEntity = entt::null;    //!< 「GAME OVER」テキスト（シーン初期化時に非表示で作る）
        Tsukino::ECS::Entity retryTextEntity = entt::null;    //!< 「Press SPACE to Retry」テキスト

        float deathElapsed = 0.0f;         //!< isDeadが立ってからの経過時間（秒）
        bool  overlayShown = false;        //!< GAME OVERテキストを表示済みか
    };
}    // namespace CombatAndroid::ECS
