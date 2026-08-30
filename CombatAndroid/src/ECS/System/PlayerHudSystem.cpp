//-------------------------------------------------------------
//! @file   PlayerHudSystem.cpp
//! @brief  PlayerHudSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PlayerHudSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerHudComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerExperienceComponent.hpp>
#include <CombatAndroid/ECS/Component/RunClockComponent.hpp>

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

        //! 危険度テキストの基準拡大率。昇格演出はこの値を一時的に上回る
        constexpr float kDangerRankFontScale = 1.0f;

        //! 危険度テキストの色が赤へ振り切るランク。ランク数に上限が無いため、
        //! これ以上は色が変わらない（色で段を数えさせる意図は無く、危険さの気配だけ伝える）
        constexpr float kDangerRankColorFull = 10.0f;

        //! 昇格直後に文字を大きくする割合。0.45で最大1.45倍
        constexpr float kRankUpFlashScaleGain = 0.45f;

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
        auto view = registry.View<PlayerComponent, HealthComponent, PlayerExperienceComponent, PlayerHudComponent, RunClockComponent>();
        for(entt::entity entity : view) {
            const auto& health = view.get<HealthComponent>(entity);
            const auto& exp     = view.get<PlayerExperienceComponent>(entity);
            auto&       hud     = view.get<PlayerHudComponent>(entity);
            const auto& clock   = view.get<RunClockComponent>(entity);

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
            // 生存時間：mm:ss形式で画面上部中央に表示する。
            // 加算はRunClockSystemが行うため、ここは表示だけを受け持つ
            //-------------------------------------------------------------
            if(hud.survivalTimeTextEntity != entt::null) {
                if(auto* survivalTimeFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(hud.survivalTimeTextEntity)) {
                    int totalSeconds = static_cast<int>(clock.elapsedSeconds);
                    int minutes       = totalSeconds / 60;
                    int seconds       = totalSeconds % 60;

                    std::wstring minutesText = std::to_wstring(minutes);
                    std::wstring secondsText = std::to_wstring(seconds);
                    if(secondsText.size() < 2)
                        secondsText.insert(0, L"0");

                    survivalTimeFont->text = minutesText + L":" + secondsText;
                }
            }

            //-------------------------------------------------------------
            // 危険度：生存時間の真下に出す。時間が経つほど敵が強くなることを
            // プレイヤーへ見せておかないと、難易度上昇が原因不明の理不尽になる。
            // 生存時間の「横」ではなく「下」に置くのは、横並びにすると
            // "12:34" の描画幅を知る必要があり、桁が増えた瞬間に重なるため
            //-------------------------------------------------------------
            if(hud.dangerRankTextEntity != entt::null) {
                if(auto* rankFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(hud.dangerRankTextEntity)) {
                    rankFont->text = L"危険度 " + std::to_wstring(clock.dangerRank);

                    // ランクが上がるほど白→赤へ寄せる。段数に上限が無いので
                    // kDangerRankColorFullで頭打ちにする
                    float rankT = std::clamp(static_cast<float>(clock.dangerRank - 1) / kDangerRankColorFull, 0.0f, 1.0f);
                    rankFont->color = hlslpp::float4(1.0f, 1.0f - 0.65f * rankT, 1.0f - 0.75f * rankT, 1.0f);
                }

                //-----------------------------------------------------
                // 昇格直後だけ文字を一瞬大きくする。FontRendererSystemは
                // worldMatrixのX軸長を拡大率として読むため、TransformComponent::scaleを
                // 書けばよい（GameLogSystemのスライドインと同じ作法）
                //-----------------------------------------------------
                if(auto* rankTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(hud.dangerRankTextEntity)) {
                    float flash = std::clamp(clock.rankUpFlashTimer / kRankUpFlashDuration, 0.0f, 1.0f);
                    float scale = kDangerRankFontScale * (1.0f + kRankUpFlashScaleGain * flash * flash);

                    rankTransform->scale = hlslpp::float3(scale, scale, 1.0f);
                    rankTransform->dirty = true;
                }
            }
        }
    }
}    // namespace CombatAndroid::ECS
