//-------------------------------------------------------------
//! @file   DamageNumberComponent.hpp
//! @brief  DamageNumberComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! @brief 同時に表示できるダメージ数値の最大数（＝プールするエンティティ数）
    constexpr int kDamageNumberPoolSize = 24;

    //-------------------------------------------------------------
    //! @struct DamageNumberComponent
    //! @brief  ダメージ数値ポップ1つ分のスロット状態。
    //!         TransformComponent／WorldAnchorComponent／FontComponentと一緒に付け、
    //!         DamageNumberSystemが毎フレーム中身を書き換える。
    //!         実行時にエンティティを生成せず、シーン生成時に作った
    //!         kDamageNumberPoolSize個を使い回すためのフラグとタイマーを持つ
    //-------------------------------------------------------------
    struct DamageNumberComponent {
        bool  active   = false;    //!< 使用中か（falseなら空きスロット）
        float elapsed  = 0.0f;     //!< 表示開始からの経過時間（秒）
        float lifetime = 0.0f;     //!< 消えるまでの総時間（秒）

        hlslpp::float4 baseColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);    //!< フェード前の基準色
    };
}    // namespace CombatAndroid::ECS
