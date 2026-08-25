//-------------------------------------------------------------
//! @file   EnemyAttackHitboxComponent.hpp
//! @brief  EnemyAttackHitboxComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Engine/Asset/AssetHandle.hpp>
#include <Tsukino/Core/typedef.hpp>

#include <hlsl++.h>

#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EnemyAttackHitboxComponent
    //! @brief  敵の攻撃モーションに合わせてプレイヤーへダメージを与える当たり判定。
    //!         WeaponComponentの手ボーン追従と同じ規約（handBoneName/handBoneNodeIndex/
    //!         resolvedAgainstModel）で手ボーンの位置を解決し、そこを中心とした球で
    //!         プレイヤーのカプセル（CharacterControllerComponent）との幾何判定を行う
    //!         （CombatSystem::Update参照。Jolt物理のクエリには依存しない）。
    //!         判定はEnemyAnimationSetComponent::currentStateがAttackの間、
    //!         attackTimerがhitStartTime〜hitStartTime+hitDurationの範囲でのみ有効になる
    //-------------------------------------------------------------
    struct EnemyAttackHitboxComponent {
        std::string handBoneName      = "mixamorig:RightHand";    //!< 判定を出すボーン名
        u32          handBoneNodeIndex = UINT32_MAX;                //!< 解決済みノードindex（未解決/見つからない場合はUINT32_MAX）
        Tsukino::Asset::AssetHandle resolvedAgainstModel;           //!< 最後にボーン解決を行った時点のモデル（比較して再解決要否を判定する）

        hlslpp::float3 boneLocalOffset{0.0f, 0.0f, 0.0f};    //!< 手ボーンローカル空間での判定中心オフセット

        float radius       = 45.0f;    //!< 判定球の半径
        float damage        = 15.0f;    //!< プレイヤーに与えるダメージ
        float hitStartTime = 0.40f;    //!< Attackステートへ入ってからの経過秒。ここから判定が有効になる
        float hitDuration   = 0.20f;    //!< 判定の有効時間（秒）

        bool hasLandedThisAttack = false;    //!< この攻撃で既に当てたか（Attackへの遷移時にクリアする）

        //-------------------------------------------------------------
        // 手ボーンの前フレーム位置。速い振り・フレーム落ちで判定をすり抜けないよう、
        // 判定窓の間はprevHandPosition→今フレームの手位置の線分でスイープ判定する
        // （CombatSystem::Update参照）。Attackへの遷移時にhasPrevHandPositionをfalseへ戻し、
        // 判定窓に入った最初のフレームだけは点判定（スイープ無し）にフォールバックする
        //-------------------------------------------------------------
        hlslpp::float3 prevHandPosition{0.0f, 0.0f, 0.0f};    //!< 前フレームの手ボーンのワールド位置
        bool           hasPrevHandPosition = false;             //!< prevHandPositionが有効か
    };
}    // namespace CombatAndroid::ECS
