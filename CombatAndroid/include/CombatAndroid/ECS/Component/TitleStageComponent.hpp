//-------------------------------------------------------------
//! @file   TitleStageComponent.hpp
//! @brief  TitleStageComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <hlsl++.h>

#include <array>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! タイトルで見せる武器の本数（武器の種類ぶん）
    inline constexpr int kTitleStageWeaponCount = static_cast<int>(WeaponId::Count);

    //-------------------------------------------------------------
    //! @struct TitleStageWeapon
    //! @brief  地面から抜けて宙に浮く武器1本ぶんの状態
    //-------------------------------------------------------------
    struct TitleStageWeapon {
        Tsukino::ECS::Entity entity = entt::null;    //!< 武器のモデルを持つエンティティ

        hlslpp::float3 groundPosition{0.0f, 0.0f, 0.0f};    //!< 刺さっている場所（地面。Yは抜ける前の沈み具合）
        float          hoverHeight = 150.0f;                //!< 抜けきったあとの高さ（地面からの距離）
        float          burstTime   = 0.0f;                  //!< シーン開始から何秒後に抜け始めるか
        float          spinPhase   = 0.0f;                  //!< 浮いている間の回転の初期位相（本ごとにずらす）
        float          bobPhase    = 0.0f;                  //!< 浮いている間の上下動の初期位相

        bool burst = false;    //!< 土煙と効果音を出したか（1本につき1回だけ）
    };

    //-------------------------------------------------------------
    //! @struct TitleStageComponent
    //! @brief  タイトル画面の3D演出（武器の登場とカメラの揺らぎ）の状態。
    //!         タイトル画面に1つだけ置く目印のエンティティに付ける
    //! @note   進行はTitleStageSystemが持つ。メニュー操作とは独立して進み、
    //!         3本とも浮き上がったら、あとは漂うだけになる（繰り返さない）
    //-------------------------------------------------------------
    struct TitleStageComponent {
        float elapsed = 0.0f;    //!< シーンが始まってからの経過秒数

        std::array<TitleStageWeapon, kTitleStageWeaponCount> weapons{};    //!< 見せる武器

        Tsukino::ECS::Entity cameraEntity = entt::null;    //!< 揺らすカメラ

        hlslpp::float3 cameraBasePosition{0.0f, 0.0f, 0.0f};    //!< カメラの基準位置（ここを中心に揺らす）
        hlslpp::float3 cameraLookAt{0.0f, 0.0f, 0.0f};          //!< カメラの注視点
    };
}    // namespace CombatAndroid::ECS
