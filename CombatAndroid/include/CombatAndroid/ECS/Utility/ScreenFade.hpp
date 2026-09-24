//-------------------------------------------------------------
//! @file   ScreenFade.hpp
//! @brief  場面の切り替わりを黒で繋ぐ共通処理の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/ScreenFadeComponent.hpp>

#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <functional>
#include <memory>

namespace Tsukino::EngineIntegration {
    struct EngineContext;
    class GameSceneBase;
}    // namespace Tsukino::EngineIntegration

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //! 次のシーンを作る関数。暗転しきった瞬間に呼ばれる
    using ScreenFadeSceneFactory = std::function<std::unique_ptr<Tsukino::EngineIntegration::GameSceneBase>()>;

    //-------------------------------------------------------------
    //! @brief  黒フェード用のエンティティを作り、黒から明ける状態で始める
    //! @param  registry [in,out] ECSレジストリ
    //! @param  context  [in]     エンジンコンテキスト
    //! @note   各シーンの初期化で1回だけ呼ぶ。併せてScreenFadeSystemの登録が要る
    //-------------------------------------------------------------
    void CreateScreenFade(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context);

    //-------------------------------------------------------------
    //! @brief  黒く覆ってから次のシーンへ切り替えるよう頼む
    //! @param  registry [in,out] ECSレジストリ
    //! @param  factory  [in]     次のシーンを作る関数
    //! @note   既に暗転中なら何もしない（連打で二重に切り替わらない）。
    //!         シーンを切り替えたい側は、ChangeSceneを直接呼ばずにこれを使う
    //-------------------------------------------------------------
    void RequestSceneChangeWithFade(Tsukino::ECS::Registry& registry, ScreenFadeSceneFactory factory);

    //-------------------------------------------------------------
    //! @brief  黒く覆っている最中か
    //! @param  registry [in] ECSレジストリ
    //! @return 暗転中ならtrue
    //! @note   メニュー側はこれを見て入力を止める（暗転中の操作を受け付けない）
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsScreenFadingOut(Tsukino::ECS::Registry& registry);
}    // namespace CombatAndroid::ECS
