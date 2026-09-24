//-------------------------------------------------------------
//! @file   ScreenFade.cpp
//! @brief  場面の切り替わりを黒で繋ぐ共通処理の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <entt/entt.hpp>

#include <utility>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief 黒フェード用のエンティティを作り、黒から明ける状態で始める
    //-------------------------------------------------------------
    void CreateScreenFade(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context) {
        Tsukino::ECS::Entity entity = registry.CreateEntity();

        ScreenFadeComponent& fade = registry.AddComponent<ScreenFadeComponent>(entity);
        fade.state                = ScreenFadeState::FadingIn;
        fade.elapsed              = 0.0f;

        // 板の位置と濃さは毎フレームScreenFadeSystemが書く（ウィンドウの大きさに追従させるため）
        fade.panelEntity = CreateUiRectEntity(registry, context, CombatAndroid::UI::kScreenFade);
    }

    //-------------------------------------------------------------
    //! @brief 黒く覆ってから次のシーンへ切り替えるよう頼む
    //-------------------------------------------------------------
    void RequestSceneChangeWithFade(Tsukino::ECS::Registry& registry, ScreenFadeSceneFactory factory) {
        auto view = registry.View<ScreenFadeComponent>();
        for(entt::entity entity : view) {
            ScreenFadeComponent& fade = view.get<ScreenFadeComponent>(entity);

            // 既に暗転中なら先の頼みを優先する（連打で行き先が入れ替わらないように）
            if(fade.state == ScreenFadeState::FadingOut)
                return;

            fade.state     = ScreenFadeState::FadingOut;
            fade.elapsed   = 0.0f;
            fade.nextScene = std::move(factory);
            fade.handedOff = false;
            return;
        }
    }

    //-------------------------------------------------------------
    //! @brief 黒く覆っている最中か
    //-------------------------------------------------------------
    bool IsScreenFadingOut(Tsukino::ECS::Registry& registry) {
        bool fading = false;

        auto view = registry.View<ScreenFadeComponent>();
        view.each([&](entt::entity, const ScreenFadeComponent& fade) {
            fading = fading || fade.state == ScreenFadeState::FadingOut;
        });

        return fading;
    }
}    // namespace CombatAndroid::ECS
