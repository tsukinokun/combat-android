//-------------------------------------------------------------
//! @file   ExpOrbComponent.hpp
//! @brief  ExpOrbComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>

#include <cstdint>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! @brief 同時に表示できるEXP玉の最大数（＝プールするエンティティ数）
    constexpr int kExpOrbPoolSize = 32;

    //-------------------------------------------------------------
    //! @enum   ExpOrbState
    //! @brief  EXP玉1個の演出状態
    //-------------------------------------------------------------
    enum class ExpOrbState : std::uint8_t {
        Falling = 0,    // 敵の死亡位置からポップアウトし、重力で落下・着地するまで
        Homing,         // 着地後、プレイヤーへ吸い寄せられていく間
        Absorbed,       // 吸収済み（次のUpdateで非表示化してプールへ戻す）
    };

    //-------------------------------------------------------------
    //! @struct ExpOrbComponent
    //! @brief  EXP玉1つ分のスロット状態。TransformComponent／WorldAnchorComponent／
    //!         SpriteComponentと一緒に付け、ExpOrbSystemが毎フレーム中身を書き換える。
    //!         DamageNumberComponentと同じく、実行時にエンティティを生成せず、
    //!         シーン生成時に作ったkExpOrbPoolSize個を使い回すためのフラグを持つ
    //-------------------------------------------------------------
    struct ExpOrbComponent {
        bool        active     = false;                  //!< 使用中か（falseなら空きスロット）
        ExpOrbState state      = ExpOrbState::Falling;    //!< 現在の演出状態
        int         expValue   = 0;                       //!< 吸収時にプレイヤーへ加算するEXP量
        float       stateTimer = 0.0f;                     //!< 現在の状態に入ってからの経過時間（秒）

        // 実際のワールド座標。TransformComponent.positionはWorldAnchorSystemが
        // 画面ピクセル座標へ上書きしてしまうため、3D座標はこちらで別管理する
        hlslpp::float3 worldPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);

        hlslpp::float3 velocity = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< Falling中のみ使用する速度
        float          groundY  = 0.0f;                                 //!< Falling中の着地高度（スポーン地点のY）
    };
}    // namespace CombatAndroid::ECS
