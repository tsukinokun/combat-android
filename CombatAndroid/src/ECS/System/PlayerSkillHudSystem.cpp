//-------------------------------------------------------------
//! @file   PlayerSkillHudSystem.cpp
//! @brief  PlayerSkillHudSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PlayerSkillHudSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillHudComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>

#include <Tsukino/Core/Path.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // レイアウトのチューニング値（全て画面ピクセル単位・画面左上基準）。
        // 先頭の2つはHP/EXPバー（PlayerHudSystem.cpp）の真下へ続けて並べるための値なので、
        // 向こうのkHpBarLeftX・EXPバーの下端を動かしたらここも合わせること
        //-------------------------------------------------------------
        constexpr float kSkillListLeftX = 24.0f;    //!< 一覧の左端。kHpBarLeftXと揃えている
        constexpr float kSkillListTopY  = 70.0f;    //!< 1行目の上端。EXPバーの下端(58)＋余白12

        constexpr float kIconSize     = 28.0f;                  //!< アイコン枠の一辺
        constexpr float kRowGapY      = 6.0f;                   //!< 行同士の縦の隙間
        constexpr float kRowPitch     = kIconSize + kRowGapY;    //!< 1行ぶんの送り
        constexpr float kIconTextGapX = 8.0f;                   //!< アイコン枠の右端から文字までの余白

        //! 文字の拡大率（TransformComponent::scale.xがそのままフォントサイズになる）
        constexpr float kSkillFontScale = 0.8f;

        const hlslpp::float4 kSkillTextColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);

        //-------------------------------------------------------------
        //! @brief  エンティティを非表示にする関数
        //! @param  registry [in] ECSレジストリ
        //! @param  entity   [in] 対象のエンティティ
        //! @note   SpriteRenderSystemは極小スケールを、FontRendererSystemは空文字を
        //!         それぞれ描画対象から外すため、この2つを書けば消える（SkillSelectSystemと同じ流儀）
        //-------------------------------------------------------------
        void HideEntity(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
            if(entity == entt::null)
                return;

            if(auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity)) {
                transform->scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
                transform->dirty = true;
            }
            if(auto* font = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity))
                font->text = L"";
        }

        //-------------------------------------------------------------
        //! @brief  アイコン枠のスプライトを指定のピクセル矩形いっぱいに広げる関数
        //! @param  registry  [in] ECSレジストリ
        //! @param  context   [in] エンジンコンテキスト（テクスチャの実寸を引くのに使う）
        //! @param  entity    [in] 対象のエンティティ
        //! @param  centerX   [in] 矩形中心のスクリーンX
        //! @param  centerY   [in] 矩形中心のスクリーンY
        //! @param  size      [in] 矩形の一辺（ピクセル）
        //! @param  tintColor [in] スプライトに乗算する色
        //! @note   SpriteRenderSystemは「テクスチャの実ピクセル数 × transform.scale」を
        //!         最終的な大きさとして使うため、テクスチャサイズで割った値をscaleへ入れる。
        //!         HPバーのようにWhitePixel.png（4x4）決め打ちにせずアセットから実寸を引いているので、
        //!         後からスキル専用のアイコン画像へ差し替えても枠の大きさが崩れない
        //!         （SkillSelectSystem.cppのStretchSpriteと同じ計算）
        //-------------------------------------------------------------
        void StretchIconSprite(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                               Tsukino::ECS::Entity entity, float centerX, float centerY, float size,
                               const hlslpp::float4& tintColor) {
            if(entity == entt::null)
                return;

            auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            auto* sprite    = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(entity);
            if(!transform || !sprite)
                return;

            float textureWidth  = 1.0f;
            float textureHeight = 1.0f;
            if(context.assetManager) {
                std::shared_ptr<Tsukino::Asset::TextureAsset> textureAsset =
                    std::static_pointer_cast<Tsukino::Asset::TextureAsset>(context.assetManager->Get(sprite->textureHandle));
                if(textureAsset && textureAsset->width > 0 && textureAsset->height > 0) {
                    textureWidth  = static_cast<float>(textureAsset->width);
                    textureHeight = static_cast<float>(textureAsset->height);
                }
            }

            transform->position = hlslpp::float3(centerX, centerY, 0.0f);
            transform->scale    = hlslpp::float3(size / textureWidth, size / textureHeight, 1.0f);
            transform->dirty    = true;

            sprite->tintColor = tintColor;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PlayerSkillHudSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        // 一覧はアニメーションを持たず取得段階の現在値をそのまま映すだけなので、経過時間を使わない
        (void)deltaTime;

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        auto view = registry.View<PlayerSkillComponent, PlayerSkillHudComponent>();
        for(entt::entity entity : view) {
            const auto& skills = view.get<PlayerSkillComponent>(entity);
            auto&       hud    = view.get<PlayerSkillHudComponent>(entity);

            //-------------------------------------------------------------
            // 取得済みのスキルだけをテーブル順に上から詰める。
            // 行はスキルの種類数ぶん用意してあるので、詰め先が足りなくなることはない
            //-------------------------------------------------------------
            size_t rowIndex = 0;

            for(const SkillTableEntry& entry : GetSkillTable()) {
                const int level = skills.levels[static_cast<size_t>(entry.id)];
                if(level <= 0)
                    continue;    // 未取得のスキルは一覧に出さない

                PlayerSkillHudRow& row = hud.rows[rowIndex];

                const float rowCenterY = kSkillListTopY + static_cast<float>(rowIndex) * kRowPitch + kIconSize * 0.5f;

                //-------------------------------------------------------------
                // アイコン枠。テクスチャはテーブルのパスから引く（AssetManagerがパスで
                // キャッシュするため、毎フレームLoadを呼んでも実際の読み込みは1回きり）。
                // スキルごとの絵を用意するまでは、WhitePixelをpanelColorで着色した四角になる
                //-------------------------------------------------------------
                if(row.iconEntity != entt::null && ctx->assetManager) {
                    if(auto* iconSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(row.iconEntity))
                        iconSprite->textureHandle = ctx->assetManager->Load(Tsukino::Core::Path(entry.backgroundTexturePath));
                }

                StretchIconSprite(registry, *ctx, row.iconEntity, kSkillListLeftX + kIconSize * 0.5f, rowCenterY, kIconSize,
                                  entry.panelColor);

                //-------------------------------------------------------------
                // スキル名とLv。文字はVerticalAlign::Middleで作ってあるため、
                // アイコンの中心Yをそのまま渡せば枠と行が揃う
                //-------------------------------------------------------------
                if(row.textEntity != entt::null) {
                    if(auto* textTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(row.textEntity)) {
                        textTransform->position = hlslpp::float3(kSkillListLeftX + kIconSize + kIconTextGapX, rowCenterY, 0.0f);
                        textTransform->scale    = hlslpp::float3(kSkillFontScale, kSkillFontScale, 1.0f);
                        textTransform->dirty    = true;
                    }
                    if(auto* textFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(row.textEntity)) {
                        textFont->text  = std::wstring(entry.displayName) + L" Lv." + std::to_wstring(level);
                        textFont->color = kSkillTextColor;
                    }
                }

                ++rowIndex;
            }

            //-------------------------------------------------------------
            // 使わなかった行を消す。1つも取得していない開始直後は全行がここを通る
            //-------------------------------------------------------------
            for(size_t i = rowIndex; i < hud.rows.size(); ++i) {
                HideEntity(registry, hud.rows[i].iconEntity);
                HideEntity(registry, hud.rows[i].textEntity);
            }
        }
    }
}    // namespace CombatAndroid::ECS
