//-------------------------------------------------------------
//! @file   PlayerHudSystem.cpp
//! @brief  PlayerHudSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PlayerHudSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerHudComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerExperienceComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        // HPバー用テクスチャ（1辺kBarTexturePixelSizeの単色正方形）をこのピクセルサイズまで引き伸ばして表示する。
        // HealthBarSystem（敵の頭上HPバー）と同じWhitePixel.pngを流用するので、テクスチャピクセルサイズも合わせる
        constexpr float kBarTexturePixelSize = 4.0f;

        constexpr float kHpBarLeftX      = 24.0f;    //!< HPバー左端のスクリーンX（画面左上基準）
        constexpr float kHpBarTopY       = 24.0f;    //!< HPバー上端のスクリーンY
        constexpr float kHpBarWidth      = 220.0f;
        constexpr float kHpBarHeight     = 18.0f;

        constexpr float kExpBarLeftX  = kHpBarLeftX;
        constexpr float kExpBarTopY   = kHpBarTopY + kHpBarHeight + 6.0f;    //!< HPバーのすぐ下に並べる
        constexpr float kExpBarWidth  = kHpBarWidth;
        constexpr float kExpBarHeight = 10.0f;

        constexpr float kTextGapX = 12.0f;    //!< バー右端からテキストまでの余白

        const hlslpp::float4 kBarBackgroundColor = hlslpp::float4(0.12f, 0.12f, 0.12f, 0.85f);    //!< 背景（暗いグレー半透明）
        const hlslpp::float4 kExpBarFillColor    = hlslpp::float4(0.35f, 0.65f, 1.0f, 1.0f);       //!< EXPバーの残量色（水色）

        //-------------------------------------------------------------
        //! @brief  1本のバー（背景・残量の2エンティティ）の見た目を更新する
        //! @param  registry       [in] ECSレジストリ
        //! @param  backgroundEntity [in] 背景スプライトのエンティティ
        //! @param  fillEntity       [in] 残量スプライトのエンティティ
        //! @param  leftX            [in] バー左端のスクリーンX（左端を固定して右側から減らす）
        //! @param  topY             [in] バー上端のスクリーンY
        //! @param  width            [in] 満タン時の幅（ピクセル）
        //! @param  height           [in] 高さ（ピクセル）
        //! @param  ratio            [in] 残量比率（0〜1）
        //! @param  fillColor        [in] 残量スプライトのtintColor
        //-------------------------------------------------------------
        void UpdateBar(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity backgroundEntity, Tsukino::ECS::Entity fillEntity, float leftX,
                       float topY, float width, float height, float ratio, const hlslpp::float4& fillColor) {
            if(backgroundEntity == entt::null || fillEntity == entt::null)
                return;

            ratio = std::clamp(ratio, 0.0f, 1.0f);

            float centerY = topY + height * 0.5f;

            //-------------------------------------------------------------
            // 背景：常に満タン幅のまま表示する
            //-------------------------------------------------------------
            auto& backgroundTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(backgroundEntity);
            backgroundTransform.position = hlslpp::float3(leftX + width * 0.5f, centerY, 0.0f);
            backgroundTransform.scale    = hlslpp::float3(width / kBarTexturePixelSize, height / kBarTexturePixelSize, 1.0f);
            backgroundTransform.dirty    = true;

            if(auto* backgroundSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(backgroundEntity))
                backgroundSprite->tintColor = kBarBackgroundColor;

            //-------------------------------------------------------------
            // 残量：ratio分だけ幅を縮める。左端をleftXに固定したいので、
            // スプライトの中心（position）をratioに応じて左へ寄せる
            //-------------------------------------------------------------
            auto& fillTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(fillEntity);
            fillTransform.position = hlslpp::float3(leftX + width * ratio * 0.5f, centerY, 0.0f);
            fillTransform.scale    = hlslpp::float3(width * ratio / kBarTexturePixelSize, height / kBarTexturePixelSize, 1.0f);
            fillTransform.dirty    = true;

            if(auto* fillSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(fillEntity))
                fillSprite->tintColor = fillColor;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PlayerHudSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto view = registry.View<PlayerComponent, HealthComponent, PlayerExperienceComponent, PlayerHudComponent>();
        for(entt::entity entity : view) {
            const auto& health = view.get<HealthComponent>(entity);
            const auto& exp     = view.get<PlayerExperienceComponent>(entity);
            auto&       hud     = view.get<PlayerHudComponent>(entity);

            //-------------------------------------------------------------
            // HPバー：残量に応じて緑→赤へ補間する（HealthBarSystemと同じ考え方）
            //-------------------------------------------------------------
            float hpRatio = health.maxHealth > 0.0f ? health.currentHealth / health.maxHealth : 0.0f;
            hlslpp::float4 hpColor(1.0f - std::clamp(hpRatio, 0.0f, 1.0f), std::clamp(hpRatio, 0.0f, 1.0f), 0.0f, 1.0f);
            UpdateBar(registry, hud.hpBarBackgroundEntity, hud.hpBarFillEntity, kHpBarLeftX, kHpBarTopY, kHpBarWidth, kHpBarHeight, hpRatio,
                      hpColor);

            //-------------------------------------------------------------
            // EXPバー
            //-------------------------------------------------------------
            float expRatio = exp.requiredExp > 0 ? static_cast<float>(exp.currentExp) / static_cast<float>(exp.requiredExp) : 0.0f;
            UpdateBar(registry, hud.expBarBackgroundEntity, hud.expBarFillEntity, kExpBarLeftX, kExpBarTopY, kExpBarWidth, kExpBarHeight,
                      expRatio, kExpBarFillColor);

            //-------------------------------------------------------------
            // 数値テキスト
            //-------------------------------------------------------------
            if(hud.hpTextEntity != entt::null) {
                if(auto* hpFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(hud.hpTextEntity)) {
                    hpFont->text = L"HP " + std::to_wstring(static_cast<int>(health.currentHealth + 0.5f)) + L" / "
                                   + std::to_wstring(static_cast<int>(health.maxHealth + 0.5f));
                }
                if(auto* hpTextTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(hud.hpTextEntity)) {
                    hpTextTransform->position =
                        hlslpp::float3(kHpBarLeftX + kHpBarWidth + kTextGapX, kHpBarTopY + kHpBarHeight * 0.5f, 0.0f);
                    hpTextTransform->dirty = true;
                }
            }

            if(hud.expTextEntity != entt::null) {
                if(auto* expFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(hud.expTextEntity)) {
                    expFont->text = L"Lv." + std::to_wstring(exp.level) + L"  " + std::to_wstring(exp.currentExp) + L" / "
                                    + std::to_wstring(exp.requiredExp);
                }
                if(auto* expTextTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(hud.expTextEntity)) {
                    expTextTransform->position =
                        hlslpp::float3(kExpBarLeftX + kExpBarWidth + kTextGapX, kExpBarTopY + kExpBarHeight * 0.5f, 0.0f);
                    expTextTransform->dirty = true;
                }
            }

            //-------------------------------------------------------------
            // 生存時間：死亡していない間だけ加算し、mm:ss形式で画面上部中央に表示する
            //-------------------------------------------------------------
            if(!health.isDead)
                hud.survivalTime += deltaTime;

            if(hud.survivalTimeTextEntity != entt::null) {
                if(auto* survivalTimeFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(hud.survivalTimeTextEntity)) {
                    int totalSeconds = static_cast<int>(hud.survivalTime);
                    int minutes       = totalSeconds / 60;
                    int seconds       = totalSeconds % 60;

                    std::wstring minutesText = std::to_wstring(minutes);
                    std::wstring secondsText = std::to_wstring(seconds);
                    if(secondsText.size() < 2)
                        secondsText.insert(0, L"0");

                    survivalTimeFont->text = minutesText + L":" + secondsText;
                }
            }
        }
    }
}    // namespace CombatAndroid::ECS
