//-------------------------------------------------------------
//! @file   PlayerHudComponent.hpp
//! @brief  PlayerHudComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerHudComponent
    //! @brief  画面左上のHP/EXPバー一式（背景・残量スプライト、テキスト）の
    //!         エンティティ参照をまとめて持つコンポーネント。プレイヤーエンティティに付け、
    //!         PlayerHudSystemが毎フレーム参照先の見た目を更新する。
    //!         HealthComponent::hpBarBackgroundEntity（敵の頭上HPバー）とは別物で、
    //!         こちらはWorldAnchorComponentを使わない固定ピクセル座標のHUDになる
    //-------------------------------------------------------------
    struct PlayerHudComponent {
        Tsukino::ECS::Entity hpBarBackgroundEntity  = entt::null;
        Tsukino::ECS::Entity hpBarFillEntity        = entt::null;
        Tsukino::ECS::Entity hpTextEntity           = entt::null;

        Tsukino::ECS::Entity expBarBackgroundEntity = entt::null;
        Tsukino::ECS::Entity expBarFillEntity       = entt::null;
        Tsukino::ECS::Entity expTextEntity          = entt::null;

        Tsukino::ECS::Entity survivalTimeTextEntity = entt::null;    //!< 画面上部中央の生存時間テキスト
        float                survivalTime           = 0.0f;         //!< 生存時間（秒）。死亡すると加算を止める
    };
}    // namespace CombatAndroid::ECS
