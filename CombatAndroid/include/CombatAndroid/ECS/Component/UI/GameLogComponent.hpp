//-------------------------------------------------------------
//! @file   GameLogComponent.hpp
//! @brief  GameLogComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <entt/entt.hpp>
#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! @brief 同時に表示できる取得ログの最大数（＝プールするスロット数）
    constexpr int kGameLogPoolSize = 6;

    //-------------------------------------------------------------
    //! @struct GameLogComponent
    //! @brief  取得ログ1行分のスロット状態。
    //!         1行は「パネル・アクセントバー・種別ラベル・主題」の4エンティティで出来ており、
    //!         このコンポーネントは根であるパネルのエンティティ
    //!         （TransformComponent／SpriteComponentと同居）に付ける。
    //!         残り3つはPlayerHudComponentと同じくエンティティ参照で持ち、
    //!         GameLogSystemが毎フレームまとめて書き換える。
    //!         ダメージ数値と同じく実行時のエンティティ生成・破棄は一切行わず、
    //!         シーン生成時に作ったkGameLogPoolSize個を使い回す
    //-------------------------------------------------------------
    struct GameLogComponent {
        bool         active   = false;    //!< 使用中か（falseなら空きスロット）
        float        elapsed  = 0.0f;     //!< 表示開始からの経過時間（秒）
        unsigned int sequence = 0;        //!< 発生順。大きいほど新しく、段の並べ替えに使う

        //-------------------------------------------------------------
        // 実際に描いている段のY（画面ピクセル）。新しい行が入って段が繰り上がっても
        // 瞬間移動させず、目標のYへ指数減衰で追従させるために現在値を保持する
        //-------------------------------------------------------------
        float slotY            = 0.0f;
        bool  slotYInitialized = false;    //!< 表示開始の1回だけ目標Yを直接代入するためのフラグ

        //! 種別色。アクセントバーのtintColorと種別ラベルの文字色で共有する
        hlslpp::float4 accentColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);

        Tsukino::ECS::Entity accentEntity = entt::null;    //!< 左端の種別色バー
        Tsukino::ECS::Entity labelEntity  = entt::null;    //!< 1行目（小さい種別ラベル）
        Tsukino::ECS::Entity textEntity   = entt::null;    //!< 2行目（主題）
    };
}    // namespace CombatAndroid::ECS
