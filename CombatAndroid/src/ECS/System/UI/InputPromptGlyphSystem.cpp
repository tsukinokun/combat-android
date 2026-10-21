//-------------------------------------------------------------
//! @file   InputPromptGlyphSystem.cpp
//! @brief  InputPromptGlyphSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/InputPromptGlyphSystem.hpp>
#include <CombatAndroid/ECS/Component/UI/InputPromptGlyphComponent.hpp>
#include <CombatAndroid/ECS/Utility/Input/GameInput.hpp>
#include <CombatAndroid/ECS/Utility/UI/InputPromptWidget.hpp>

#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void InputPromptGlyphSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->inputSystem)
            return;

        const Tsukino::Input::InputSystem& input = *ctx->inputSystem;

        auto view = registry.View<InputPromptGlyphComponent, Tsukino::BuiltIn::ECS::FontComponent>();
        view.each([&](InputPromptGlyphComponent& glyph, Tsukino::BuiltIn::ECS::FontComponent& font) {
            // マウスの絵で見せている間は文字を出さない（LayoutPromptが消している）
            if(IsActionShownAsMouse(input, glyph.action))
                return;

            const std::wstring label = GetActionLabel(input, glyph.action);
            if(font.text != label)
                font.text = label;

            //-------------------------------------------------------------
            // 面の色。不透明度は各画面がフェードに使っているので、色だけを差し替える
            //-------------------------------------------------------------
            if(auto* face = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(glyph.faceEntity)) {
                const hlslpp::float4 faceColor = GetPromptFaceColor(input, glyph.action);
                face->tintColor = hlslpp::float4(faceColor.xyz, face->tintColor.w);
            }
        });
    }
}    // namespace CombatAndroid::ECS
