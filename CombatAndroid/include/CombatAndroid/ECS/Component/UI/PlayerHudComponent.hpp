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

        //! 生存時間テキストの真下に出す危険度テキスト。
        //! 表示する値はRunClockComponentが持つ（生存時間もそちらへ1本化したため、
        //! ここには時間を数えるメンバは無い）
        Tsukino::ECS::Entity dangerRankTextEntity = entt::null;

        //! HPバーの「減った分」。残量の後ろに敷く白い帯で、被弾すると少し待ってから残量まで縮む
        Tsukino::ECS::Entity hpBarTrailEntity = entt::null;

        //-------------------------------------------------------------
        // バーと数値の動きのための状態（PlayerHudSystemだけが読み書きする）
        //-------------------------------------------------------------
        float trailRatio       = -1.0f;    //!< 「減った分」の帯の今の長さ（HPの割合。負なら未初期化）
        float trailHoldTimer   = 0.0f;     //!< 被弾してから帯が縮み始めるまでの残り秒数
        float shownExpRatio    = -1.0f;    //!< EXPバーの今の見た目の長さ（実際の割合へなめらかに追う。負なら未初期化）
        float lastHealth       = -1.0f;    //!< 前フレームのHP（被弾・回復の検出）
        int   lastLevel        = -1;       //!< 前フレームのレベル（レベルアップの検出）
    };
}    // namespace CombatAndroid::ECS
