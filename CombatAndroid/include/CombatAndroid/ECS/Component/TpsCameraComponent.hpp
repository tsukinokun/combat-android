//-------------------------------------------------------------
//! @file   TpsCameraComponent.hpp
//! @brief  TpsCameraComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct TpsCameraComponent
    //! @brief  対象（プレイヤー）を中心にマウスで旋回する三人称視点カメラのパラメータ
    //-------------------------------------------------------------
    struct TpsCameraComponent {
        Tsukino::ECS::Entity target = entt::null;    //!< 追従対象のエンティティ（プレイヤー）

        float distance  = 400.0f;    //!< 対象からのカメラの距離
        // 対象のTransform位置は足元（CharacterControllerComponent.centerOffset採用後）を表す。
        // 頭の少し上あたりを狙うため、身長210cm相当を見込んだオフセットにしている
        float height     = 140.0f;    //!< 対象の位置（足元）からの基準高さオフセット（頭の少し上あたりを狙う）
        float lookHeight = 215.0f;    //!< 注視点の高さオフセット（頭の少し上あたりを見る）

        // --- 位置追従のばね ---
        // 指数減衰で追うと目標を決して行き過ぎないため、走り出し・急停止・振り向きで
        // カメラが「遅れて止まる」だけになり重さが出ない。減衰比を1未満にすると
        // 目標をわずかに行き過ぎてから戻り、ばねで吊られた手触りになる。
        // 既定値は旧来の指数減衰（速度8）と一定速度で追うときの遅れがほぼ同じになるよう選んである
        float followSpringFrequency     = 2.0f;       //!< 固有振動数（Hz）。大きいほど素早く追う
        float followSpringDamping       = 0.75f;      //!< 減衰比。1.0で行き過ぎ無し、小さいほど揺り戻す
        float followSpringResetDistance = 1500.0f;    //!< 目標とこれ以上離れたら（リトライ等）ばねを使わず目標へ置き直す

        hlslpp::float3 followSpringVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 位置追従の速度（内部状態）。これを持ち越すから行き過ぎが表現できる
        bool           hasFollowSpringState = false;                               //!< ばねの状態が初期化済みか（内部状態）

        // --- 被弾時の揺れ（ばね） ---
        // 被弾の瞬間に、ばねで0へ引き戻される注視点のずれへ初速を与える。
        // 初速を与えるだけなので、揺れの形（振動数・減衰）はばねが決め、連続で被弾すれば
        // 速度が足し合わさって自然に揺れが強まる。
        // 注視点だけをずらすので、カメラは位置を保ったまま首を振るように揺れる
        float shakeFrequency       = 8.0f;      //!< 揺れの振動数（Hz）
        float shakeDamping         = 0.15f;     //!< 減衰比。小さいほど長く揺れ続ける（0.15で約0.3秒でほぼ収まる）
        float shakeImpulse         = 450.0f;    //!< 基準ダメージで与える初速（ワールド単位/秒）。振れ幅は 初速 / (2π × 振動数) の8割ほど（450で約7）
        float shakeReferenceDamage = 15.0f;     //!< このダメージで初速が shakeImpulse ちょうどになる（敵の基本攻撃力）
        float shakeMinScale        = 0.5f;      //!< 小ダメージでも揺れが分かるよう、ダメージによる倍率の下限
        float shakeMaxScale        = 2.5f;      //!< 大ダメージで揺れすぎないよう、ダメージによる倍率の上限

        hlslpp::float3 shakeOffset   = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 注視点のずれ（内部状態）
        hlslpp::float3 shakeVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 上記の速度（内部状態）

        // --- マウスによる旋回 ---
        float yaw   = 0.0f;    //!< 現在のカメラyaw（ラジアン。0でプレイヤーの初期正面=+Z方向を映す）
        float pitch = 0.2f;    //!< 現在のカメラpitch（ラジアン。正で見下ろし、負で見上げ）

        float mouseSensitivity = 0.0012f;    //!< マウス1ピクセル移動あたりの回転量（ラジアン）
        float minPitch          = -0.5f;      //!< pitchの下限（見上げすぎ防止）
        float maxPitch          = 1.3f;       //!< pitchの上限（見下ろしすぎ防止）

        // --- マウスキャプチャ（カーソル非表示＋中央固定）の状態 ---
        bool mouseCaptured        = true;     //!< true: カーソルを隠して旋回操作に使う（Escキーで切り替え可能）
        bool wasCapturedLastFrame = false;    //!< 直前フレームで実際にキャプチャされていたか（復帰時の誤入力防止用）
    };
}    // namespace CombatAndroid::ECS
