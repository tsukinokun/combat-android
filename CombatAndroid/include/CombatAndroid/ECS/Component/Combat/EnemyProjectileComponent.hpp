//-------------------------------------------------------------
//! @file   EnemyProjectileComponent.hpp
//! @brief  EnemyProjectileComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <entt/entt.hpp>
#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EnemyProjectileComponent
    //! @brief  敵（ボス）が撃った、プレイヤーにだけ当たる弾（蜘蛛の糸弾など）
    //! @note   プレイヤーの斬撃弾（ProjectileComponent）は敵に当たる側なので別にしてある。
    //!         EnemyProjectileSystemが毎フレーム進め、プレイヤーのカプセルに触れたら当てて消す。
    //!         ダメージと鈍足は撃った瞬間に確定させてここへ焼き込む（飛んでいる間にボスが倒れても変わらない）
    //-------------------------------------------------------------
    struct EnemyProjectileComponent {
        Tsukino::ECS::Entity owner = entt::null;                         //!< 撃った敵（ヒットストップに使う。倒れていてもよい）
        hlslpp::float3       direction = hlslpp::float3(0.0f, 0.0f, 1.0f);    //!< 進む向き（正規化済み・水平）
        float                speed       = 900.0f;    //!< 速さ（ユニット/秒）
        float                damage      = 10.0f;     //!< 当たったときのダメージ（倍率適用後）
        float                radius      = 40.0f;     //!< 当たりの半径
        float                remaining   = 1600.0f;   //!< あと進める距離。0以下で消える
        float                slowSeconds = 0.0f;      //!< 当たったときの鈍足の長さ（秒）
        float                slowScale   = 1.0f;      //!< 鈍足中の移動速度の倍率
    };
}    // namespace CombatAndroid::ECS
