//-------------------------------------------------------------
//! @file   CutsceneComponent.hpp
//! @brief  CutsceneComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <hlsl++.h>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct CutsceneShot
    //! @brief  カットシーンを構成するショット1つぶんのカメラ演出パラメータ
    //! @note   フィールド名はTpsCameraComponentに合わせてある（見比べやすくするため）
    //-------------------------------------------------------------
    struct CutsceneShot {
        Tsukino::ECS::Entity targetEntity = entt::null;                     //!< 注視・追従の基準。無効なら targetPoint を使う
        hlslpp::float3        targetPoint = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< targetEntityが無効なときの基準点

        float distance   = 400.0f;    //!< 基準からのカメラの距離
        float height      = 140.0f;    //!< 基準位置からのカメラの高さオフセット
        float lookHeight  = 215.0f;    //!< 注視点の高さオフセット
        float yaw          = 0.0f;      //!< 水平方向の向き（ラジアン）
        float pitch        = 0.2f;      //!< 見下ろし角（ラジアン。正で見下ろし）
        float fov           = 60.0f;     //!< 画角（度）
        float farZ          = 2000.0f;   //!< 遠方クリップ距離。通常のTPSカメラ（Prefab/TpsCamera）と同じ既定値

        float duration       = 2.5f;    //!< このショットに留まる秒数（実時間）
        float blendFrequency = 1.0f;    //!< 位置・注視点・画角を目標framingへ寄せるばねの速さ（Hz）。減衰比は1.0固定
        float yawDriftSpeed  = 0.0f;    //!< 保持中にyawへ足し込む回転速度（ラジアン/秒、実時間）。0で回さない
    };

    //-------------------------------------------------------------
    //! @struct CutsceneComponent
    //! @brief  カットシーン（演出カメラ）の再生状態。
    //!         PauseMenuComponent/SkillSelectComponentと同じく、UIエンティティの
    //!         ハンドルごとプレイヤーエンティティに1つだけ付ける
    //-------------------------------------------------------------
    struct CutsceneComponent {
        std::vector<CutsceneShot> shots;              //!< 再生中のショット列
        bool                       playing          = false;    //!< 再生中か
        int                        currentShotIndex = -1;       //!< 再生中のショット番号（-1で未開始）
        float                      shotTimer         = 0.0f;     //!< 現在ショット内の経過秒（実時間）

        //-----------------------------------------------------
        // ばねの内部状態（TpsCameraComponent::followSpring*と同じ考え方）。
        // ショットが切り替わっても持ち越すことで、ショット間が「カット」ではなく
        // 連続したカメラ移動に見える
        //-----------------------------------------------------
        hlslpp::float3 springPosition       = hlslpp::float3(0.0f, 0.0f, 0.0f);
        hlslpp::float3 springVelocity       = hlslpp::float3(0.0f, 0.0f, 0.0f);
        hlslpp::float3 lookAtSpringPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);
        hlslpp::float3 lookAtSpringVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);
        float          springFov            = 60.0f;
        float          springFovVelocity    = 0.0f;
        bool           hasSpringState       = false;    //!< ばねの状態が初期化済みか（初回tickでの瞬間スナップ判定に使う）

        //-----------------------------------------------------
        // レターボックス（上下の黒帯）・スキップ案内のUIエンティティ。
        // シーン初期化時に非表示で作っておき、CutsceneSystemが再生中だけ表示する
        //-----------------------------------------------------
        Tsukino::ECS::Entity letterboxTopEntity    = entt::null;
        Tsukino::ECS::Entity letterboxBottomEntity = entt::null;
        Tsukino::ECS::Entity skipHintTextEntity    = entt::null;

        //-----------------------------------------------------
        // レターボックスの退場アニメーション。playing=falseになった後も、
        // 帯だけ短い時間をかけて縮めながら消す（登場は狙ったハードカットのまま）
        //-----------------------------------------------------
        bool  retractingLetterbox   = false;    //!< 退場アニメーション中か
        float letterboxRetractTimer = 0.0f;     //!< 退場アニメーションの経過秒（実時間）
    };
}    // namespace CombatAndroid::ECS
