//-------------------------------------------------------------
//! @file   SoftTargetComponent.hpp
//! @brief  SoftTargetComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct SoftTargetComponent
    //! @brief  プレイヤーが今狙っている敵（ソフトターゲット）と、攻撃時の振り向きの状態
    //! @note   プレイヤーに "null" でアタッチする（実行時状態だけを持つ）。
    //!         ターゲットはSoftTargetSystemが毎フレーム自動で選び、TargetMarkerSystemが画面に印を出す。
    //!         攻撃の段に入るとRequestAttackTurnAssistが振り向きを要求し、AttackTurnAssistSystemが回す
    //-------------------------------------------------------------
    struct SoftTargetComponent {
        //-------------------------------------------------------------
        // ターゲット（SoftTargetSystemが書く）
        //-------------------------------------------------------------
        Tsukino::ECS::Entity target         = entt::null;    //!< 今のターゲット（いなければnull）
        bool                 assistReady    = false;         //!< 振り向きが効く範囲（assistRange）に入っているか。印の濃さにも使う
        float                switchCooldown = 0.0f;          //!< ターゲットを乗り換えてからの残り時間。0になるまで次の乗り換えをしない

        //-------------------------------------------------------------
        // 攻撃時の振り向き（RequestAttackTurnAssistが書き、AttackTurnAssistSystemが進める）
        //-------------------------------------------------------------
        bool               turning     = false;                                     //!< 振り向き中か
        hlslpp::quaternion turnFrom    = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    //!< 振り向き開始時の向き
        hlslpp::quaternion turnTo      = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    //!< ターゲットの方向の向き
        float              turnElapsed = 0.0f;                                      //!< 振り向き始めてからの経過秒
    };
}    // namespace CombatAndroid::ECS
