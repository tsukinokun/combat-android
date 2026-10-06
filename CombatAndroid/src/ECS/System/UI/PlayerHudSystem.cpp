//-------------------------------------------------------------
//! @file   PlayerHudSystem.cpp
//! @brief  PlayerHudSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/PlayerHudSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/UI/PlayerHudComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/PlayerExperienceComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/RunClockComponent.hpp>

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

        //-------------------------------------------------------------
        //! @struct PlayerHudParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/PlayerHud.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct PlayerHudParams {
            float hpBarLeftX = 24.0f;    //!< HPバー左端のスクリーンX（画面左上基準）
            float hpBarTopY = 24.0f;    //!< HPバー上端のスクリーンY
            float hpBarWidth = 220.0f;
            float hpBarHeight = 18.0f;

            // EXPバーはHPバーのすぐ下に、同じ左端・同じ幅で並べる
            float expBarGapY = 6.0f;    //!< HPバーの下端からEXPバーの上端までの隙間
            float expBarHeight = 10.0f;

            float textGapX = 12.0f;    //!< バー右端からテキストまでの余白

            //! HP・EXPの数値テキストの拡大率。2本のバーの間隔（24px）に収まる大きさにしないと、
            //! 上下の行が重なって読めなくなる（既定の1.0は危険度テキストと同じ大きさ）
            float hudTextFontScale = 0.5f;

            hlslpp::float4 barBackgroundColor = hlslpp::float4(0.12f, 0.12f, 0.12f, 0.85f);    //!< 背景（暗いグレー半透明）
            hlslpp::float4 expBarFillColor = hlslpp::float4(0.35f, 0.65f, 1.0f, 1.0f);       //!< EXPバーの残量色（水色）

            //! 危険度テキストの基準拡大率。昇格演出はこの値を一時的に上回る
            float dangerRankFontScale = 1.0f;

            //! 危険度テキストの色が赤へ振り切るランク。ランク数に上限が無いため、
            //! これ以上は色が変わらない（色で段を数えさせる意図は無く、危険さの気配だけ伝える）
            float dangerRankColorFull = 10.0f;

            //! 危険度テキストが赤へ振り切ったときに、緑と青をどれだけ落とすか（白→赤の寄せ方）
            float dangerRankGreenFade = 0.65f;
            float dangerRankBlueFade = 0.75f;

            //! 昇格直後に文字を大きくする割合。0.45で最大1.45倍
            float rankUpFlashScaleGain = 0.45f;
        };

        template <class Archive>
        void load(Archive& archive, PlayerHudParams& params) {
            LoadField(archive, "hpBarLeftX", params.hpBarLeftX);
            LoadField(archive, "hpBarTopY", params.hpBarTopY);
            LoadField(archive, "hpBarWidth", params.hpBarWidth);
            LoadField(archive, "hpBarHeight", params.hpBarHeight);
            LoadField(archive, "expBarGapY", params.expBarGapY);
            LoadField(archive, "expBarHeight", params.expBarHeight);
            LoadField(archive, "textGapX", params.textGapX);
            LoadField(archive, "hudTextFontScale", params.hudTextFontScale);
            LoadField(archive, "barBackgroundColor", params.barBackgroundColor);
            LoadField(archive, "expBarFillColor", params.expBarFillColor);
            LoadField(archive, "dangerRankFontScale", params.dangerRankFontScale);
            LoadField(archive, "dangerRankColorFull", params.dangerRankColorFull);
            LoadField(archive, "dangerRankGreenFade", params.dangerRankGreenFade);
            LoadField(archive, "dangerRankBlueFade", params.dangerRankBlueFade);
            LoadField(archive, "rankUpFlashScaleGain", params.rankUpFlashScaleGain);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const PlayerHudParams& GetParams() {
            static const PlayerHudParams s_params = LoadSystemParams<PlayerHudParams>("PlayerHud");
            return s_params;
        }

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
            const PlayerHudParams& params = GetParams();

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
                backgroundSprite->tintColor = params.barBackgroundColor;

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
        const PlayerHudParams& params = GetParams();

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
            UpdateBar(registry, hud.hpBarBackgroundEntity, hud.hpBarFillEntity, params.hpBarLeftX, params.hpBarTopY, params.hpBarWidth, params.hpBarHeight, hpRatio,
                      hpColor);

            //-------------------------------------------------------------
            // EXPバー
            //-------------------------------------------------------------
            float expRatio = exp.requiredExp > 0 ? static_cast<float>(exp.currentExp) / static_cast<float>(exp.requiredExp) : 0.0f;
            UpdateBar(registry, hud.expBarBackgroundEntity, hud.expBarFillEntity, params.hpBarLeftX, (params.hpBarTopY + params.hpBarHeight + params.expBarGapY), params.hpBarWidth, params.expBarHeight,
                      expRatio, params.expBarFillColor);

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
                        hlslpp::float3(params.hpBarLeftX + params.hpBarWidth + params.textGapX, params.hpBarTopY + params.hpBarHeight * 0.5f, 0.0f);
                    hpTextTransform->scale = hlslpp::float3(params.hudTextFontScale, params.hudTextFontScale, 1.0f);
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
                        hlslpp::float3(params.hpBarLeftX + params.hpBarWidth + params.textGapX, (params.hpBarTopY + params.hpBarHeight + params.expBarGapY) + params.expBarHeight * 0.5f, 0.0f);
                    expTextTransform->scale = hlslpp::float3(params.hudTextFontScale, params.hudTextFontScale, 1.0f);
                    expTextTransform->dirty = true;
                }
            }

            //-------------------------------------------------------------
            // 生存時間：「経過 / クリア目標」を分:秒で画面上部中央に表示する。
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

                    // クリアまでの目標も並べて、あとどれだけ生き延びればよいかを見せる
                    survivalTimeFont->text = minutesText + L":" + secondsText + L" / "
                                             + std::to_wstring(static_cast<int>(kRunClearSeconds) / 60) + L":00";
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
                    float rankT = std::clamp(static_cast<float>(clock.dangerRank - 1) / params.dangerRankColorFull, 0.0f, 1.0f);
                    rankFont->color = hlslpp::float4(1.0f, 1.0f - params.dangerRankGreenFade * rankT, 1.0f - params.dangerRankBlueFade * rankT, 1.0f);
                }

                //-----------------------------------------------------
                // 昇格直後だけ文字を一瞬大きくする。FontRendererSystemは
                // worldMatrixのX軸長を拡大率として読むため、TransformComponent::scaleを
                // 書けばよい（GameLogSystemのスライドインと同じ作法）
                //-----------------------------------------------------
                if(auto* rankTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(hud.dangerRankTextEntity)) {
                    float flash = std::clamp(clock.rankUpFlashTimer / kRankUpFlashDuration, 0.0f, 1.0f);
                    float scale = params.dangerRankFontScale * (1.0f + params.rankUpFlashScaleGain * flash * flash);

                    rankTransform->scale = hlslpp::float3(scale, scale, 1.0f);
                    rankTransform->dirty = true;
                }
            }
        }
    }
}    // namespace CombatAndroid::ECS
