//-------------------------------------------------------------
//! @file   PickupComponent.hpp
//! @brief  PickupComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>

#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PickupComponent
    //! @brief  ワールドに落ちていて拾えるエンティティに付与するコンポーネント。
    //!         PickupSystemが、草の上に浮かせる演出・リムグロー・
    //!         触れたときの自動取得をまとめて行う
    //-------------------------------------------------------------
    struct PickupComponent {
        float        radius      = 150.0f;      //!< プレイヤーがこの距離まで近づくと拾う（1ユニット≒1cm規約）
        std::wstring displayName = L"アイテム";    //!< UIに出す名前
        float        labelHeight = 120.0f;      //!< ラベルを出すアイテム原点からの高さ

        //-------------------------------------------------------------
        // 演出用の状態。PickupSystemが毎フレーム進める
        //-------------------------------------------------------------
        float rimGlowBlend = 0.0f;    //!< 0=遠くの弱い発光, 1=近くの最大の発光。プレイヤーとの距離へ滑らかに追従する
        float pulseTime    = 0.0f;    //!< 白発光の脈動・上下の漂い用の経過時間

        hlslpp::float3     restPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);            //!< 地面に置かれた位置（浮かせる基準）
        hlslpp::quaternion restRotation = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    //!< 地面に横たわっていた姿勢（浮き上がりの始点）
        bool               hasRest      = false;                                       //!< 上の2つを記録済みか（初めて扱うフレームで記録する）
        float              riseTimer    = 0.0f;                                        //!< 横たわった姿勢から浮かぶ姿勢へ起き上がるまでの経過時間
        float              spinAngle    = 0.0f;                                        //!< 浮いている間に縦軸で回している角度（ラジアン）
    };
}    // namespace CombatAndroid::ECS
