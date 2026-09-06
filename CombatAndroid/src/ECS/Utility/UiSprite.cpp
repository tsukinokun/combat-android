//-------------------------------------------------------------
//! @file   UiSprite.cpp
//! @brief  画面固定UIのスプライト・文字を配置する共通処理の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>

#include <entt/entt.hpp>

#include <memory>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @brief  スプライトが指すテクスチャの実ピクセル寸法を引く
        //! @param  context [in]  エンジンコンテキスト
        //! @param  sprite  [in]  対象のスプライト
        //! @param  width   [out] テクスチャの幅（引けなければ1のまま）
        //! @param  height  [out] テクスチャの高さ（引けなければ1のまま）
        //! @note   SpriteRenderSystemは「テクスチャの実ピクセル数 × transform.scale」を
        //!         最終的な大きさとして使うため、欲しいピクセル寸法をこれで割った値をscaleへ入れる
        //-------------------------------------------------------------
        void ResolveTextureSize(Tsukino::EngineIntegration::EngineContext& context, const Tsukino::BuiltIn::ECS::SpriteComponent& sprite,
                                float& width, float& height) {
            width  = 1.0f;
            height = 1.0f;

            if(!context.assetManager)
                return;

            std::shared_ptr<Tsukino::Asset::TextureAsset> textureAsset =
                std::static_pointer_cast<Tsukino::Asset::TextureAsset>(context.assetManager->Get(sprite.textureHandle));
            if(textureAsset && textureAsset->width > 0 && textureAsset->height > 0) {
                width  = static_cast<float>(textureAsset->width);
                height = static_cast<float>(textureAsset->height);
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief スプライトを中心座標・ピクセル寸法・色で配置する
    //-------------------------------------------------------------
    void StretchSprite(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, Tsukino::ECS::Entity entity,
                       float centerX, float centerY, float width, float height, const hlslpp::float4& tintColor) {
        StretchSpriteRotated(registry, context, entity, centerX, centerY, width, height, 0.0f, tintColor);
    }

    //-------------------------------------------------------------
    //! @brief スプライトを中心座標・ピクセル寸法・色・回転で配置する
    //-------------------------------------------------------------
    void StretchSpriteRotated(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                              Tsukino::ECS::Entity entity, float centerX, float centerY, float width, float height, float rollRadians,
                              const hlslpp::float4& tintColor) {
        if(entity == entt::null)
            return;

        auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
        auto* sprite    = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(entity);
        if(!transform || !sprite)
            return;

        float textureWidth  = 1.0f;
        float textureHeight = 1.0f;
        ResolveTextureSize(context, *sprite, textureWidth, textureHeight);

        transform->position = hlslpp::float3(centerX, centerY, 0.0f);
        transform->scale    = hlslpp::float3(width / textureWidth, height / textureHeight, 1.0f);

        // 画面は右手前向きにZ軸が伸びる想定なので、画面内の回転はZ軸まわりで表す
        transform->rotation = hlslpp::quaternion::rotation_z(rollRadians);
        transform->dirty    = true;

        sprite->tintColor = tintColor;
    }

    //-------------------------------------------------------------
    //! @brief 文字エンティティの位置・大きさ・内容を書く
    //-------------------------------------------------------------
    void PlaceUiText(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float x, float y, float fontScale,
                     const std::wstring& text, const hlslpp::float4& color) {
        if(entity == entt::null)
            return;

        auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
        auto* font      = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity);
        if(!transform || !font)
            return;

        transform->position = hlslpp::float3(x, y, 0.0f);
        transform->scale    = hlslpp::float3(fontScale, fontScale, 1.0f);
        transform->dirty    = true;

        font->text  = text;
        font->color = color;
    }

    //-------------------------------------------------------------
    //! @brief スプライトを非表示にする
    //-------------------------------------------------------------
    void HideUiSprite(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        if(entity == entt::null)
            return;

        if(auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity)) {
            transform->scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
            transform->dirty = true;
        }
    }

    //-------------------------------------------------------------
    //! @brief 文字エンティティを非表示にする
    //-------------------------------------------------------------
    void HideUiText(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        if(entity == entt::null)
            return;

        if(auto* font = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity))
            font->text.clear();
    }
}    // namespace CombatAndroid::ECS
