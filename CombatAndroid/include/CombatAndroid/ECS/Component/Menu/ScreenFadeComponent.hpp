//-------------------------------------------------------------
//! @file   ScreenFadeComponent.hpp
//! @brief  ScreenFadeComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

// std::unique_ptr<GameSceneBase> を値として持つため、前方宣言では足りない
// （unique_ptrの破棄に完全な型が要る）
#include <Tsukino/EngineIntegration/Scene/GameSceneBase.hpp>

#include <functional>
#include <memory>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum   ScreenFadeState
    //! @brief  暗転・明転の進み具合
    //-------------------------------------------------------------
    enum class ScreenFadeState {
        FadingIn,     //!< 黒から明けている最中（シーンに入った直後）
        Idle,         //!< 何も覆っていない
        FadingOut,    //!< 黒く覆っている最中。覆いきったら次のシーンへ移る
    };

    //-------------------------------------------------------------
    //! @struct ScreenFadeComponent
    //! @brief  場面の切り替わりを黒で繋ぐための状態。1シーンに1つだけ置く
    //! @note   進行はScreenFadeSystemが持つ。シーンを切り替えたい側は
    //!         RequestSceneChangeWithFade（ScreenFade.hpp）を呼ぶだけでよく、
    //!         「暗転しきってからChangeSceneする」待ちはSystem側に閉じている
    //-------------------------------------------------------------
    struct ScreenFadeComponent {
        ScreenFadeState state   = ScreenFadeState::FadingIn;    //!< 今の状態
        float           elapsed = 0.0f;                         //!< 今の状態になってからの経過秒数

        Tsukino::ECS::Entity panelEntity = entt::null;    //!< 画面全体を覆う黒い板（Sprite）

        //! 暗転しきった後に切り替える先。次のシーンは切り替える瞬間に作る
        //! （頼まれた時点で作ると、暗転の裏でそのシーンの初期化が走って画面が止まる）
        std::function<std::unique_ptr<Tsukino::EngineIntegration::GameSceneBase>()> nextScene;

        bool handedOff = false;    //!< 切り替えを頼んだか（1回だけ呼ぶための目印）
    };
}    // namespace CombatAndroid::ECS
