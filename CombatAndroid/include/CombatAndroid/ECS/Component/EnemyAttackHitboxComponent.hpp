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
    //!         WeaponComponentの手ボーン追従と同じ規約（boneName/boneNodeIndex/
    //!         resolvedAgainstModel）で指定ボーン（頭部・腕など、敵ごとに攻撃部位が異なる）の
    //!         位置を解決し、プレイヤーのカプセル（CharacterControllerComponent）との
    //!         幾何判定を行う（CombatSystem::Update参照。Jolt物理のクエリには依存しない）。
    //!         endBoneNameが空なら「boneNodeIndex位置を中心とした球」で判定する（頭部など）。
    //!         endBoneNameを設定すると「boneNodeIndex→endBoneNodeIndexを芯線とするカプセル」
    //!         で判定する（腕の振り抜きなど、1点の球では部位を表現しきれない場合用）。
    //!         判定はEnemyAnimationSetComponent::currentStateがAttackの間、
    //!         attackTimerがhitStartTime〜hitStartTime+hitDurationの範囲でのみ有効になる
    //-------------------------------------------------------------
    struct EnemyAttackHitboxComponent {
        std::string boneName      = "mixamorig:RightHand";    //!< 判定の始点（球モードでは中心）を出すボーン名
        u32          boneNodeIndex = UINT32_MAX;                //!< 解決済みノードindex（未解決/見つからない場合はUINT32_MAX）
        Tsukino::Asset::AssetHandle resolvedAgainstModel;       //!< 最後にboneNameの解決を行った時点のモデル（比較して再解決要否を判定する）

        hlslpp::float3 boneLocalOffset{0.0f, 0.0f, 0.0f};    //!< 始点ボーンローカル空間での判定中心オフセット

        //-------------------------------------------------------------
        // カプセルモード用。endBoneNameが空文字のままなら従来通りの球モードになる
        // （腕のように1点の球では部位を表現しきれない場合、始点ボーン→終点ボーンを
        //   芯線とするカプセルで判定する。CombatSystem::Update参照）
        //-------------------------------------------------------------
        std::string    endBoneName      = "";                  //!< 判定の終点（カプセルモード時のみ使用）を出すボーン名。空なら球モード
        u32            endBoneNodeIndex = UINT32_MAX;           //!< 解決済みノードindex
        hlslpp::float3 endBoneLocalOffset{0.0f, 0.0f, 0.0f};    //!< 終点ボーンローカル空間での判定終端オフセット
        Tsukino::Asset::AssetHandle resolvedAgainstModelEnd;    //!< 最後にendBoneNameの解決を行った時点のモデル
        //!< （boneName用resolvedAgainstModelとは別に持つ。同じハンドルを共有すると、
        //!<   始点解決時点でハンドルが埋まり終点が再解決されなくなるため）

        float radius       = 45.0f;    //!< 判定球（またはカプセル）の半径
        float damage        = 15.0f;    //!< プレイヤーに与えるダメージ
        float hitStartTime = 0.40f;    //!< Attackステートへ入ってからの経過秒。ここから判定が有効になる
        float hitDuration   = 0.20f;    //!< 判定の有効時間（秒）

        bool hasLandedThisAttack = false;    //!< この攻撃で既に当てたか（Attackへの遷移時にクリアする）

        //-------------------------------------------------------------
        // 判定点（球モードでは中心、カプセルモードでは終点）の前フレーム位置。
        // 速い振り・フレーム落ちで判定をすり抜けないよう、判定窓の間は
        // prevSweepPoint→今フレームの判定点の線分でスイープ判定する
        // （CombatSystem::Update参照）。Attackへの遷移時にhasPrevSweepPointをfalseへ戻し、
        // 判定窓に入った最初のフレームだけは点判定（スイープ無し）にフォールバックする
        //-------------------------------------------------------------
        hlslpp::float3 prevSweepPoint{0.0f, 0.0f, 0.0f};    //!< 前フレームの判定点のワールド位置
        bool           hasPrevSweepPoint = false;             //!< prevSweepPointが有効か
    };
}    // namespace CombatAndroid::ECS
