//-------------------------------------------------------------
//! @file   PlayerDamageEffectComponent.hpp
//! @brief  PlayerDamageEffectComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerDamageEffectComponent
    //! @brief  被弾演出（プレイヤーモデルの点滅・画面の赤フラッシュ）の実行時状態。
    //!         PlayerDamagedEventを受けてPlayerDamageEffectSystemが毎フレーム進行させる。
    //!         プレイヤーエンティティに1つだけ付ける
    //-------------------------------------------------------------
    struct PlayerDamageEffectComponent {
        //-------------------------------------------------------------
        // モデルの点滅（ModelComponent::visibleを短い間隔でトグルする）
        //-------------------------------------------------------------
        float blinkTimer      = 0.0f;     //!< 残り時間（秒）。0より大きい間だけ点滅させる
        float blinkDuration   = 0.35f;    //!< 被弾から点滅が終わるまでの時間（秒）
        float blinkInterval   = 0.06f;    //!< 可視/不可視を切り替える間隔（秒）
        float blinkPhaseTimer = 0.0f;     //!< blinkIntervalに対する経過時間（内部状態）

        //-------------------------------------------------------------
        // 画面の赤フラッシュ（screenFlashEntityのSpriteComponent::tintColor.aを減衰させる）
        //-------------------------------------------------------------
        Tsukino::ECS::Entity screenFlashEntity   = entt::null;    //!< シーン初期化時に1つ作る、画面全体を覆うスプライト
        float                 screenFlashTimer    = 0.0f;          //!< 残り時間（秒）
        float                 screenFlashDuration = 0.25f;         //!< 被弾からフェードアウトが終わるまでの時間（秒）
    };
}    // namespace CombatAndroid::ECS
