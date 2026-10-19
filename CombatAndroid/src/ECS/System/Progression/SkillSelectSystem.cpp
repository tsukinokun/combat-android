//-------------------------------------------------------------
//! @file   SkillSelectSystem.cpp
//! @brief  SkillSelectSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Progression/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Progression/SkillSelectComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/HitStopComponent.hpp>
#include <CombatAndroid/ECS/Component/Weapon/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Event/UI/GameLogEvent.hpp>
#include <CombatAndroid/ECS/Event/Audio/SoundEvent.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/RunResultSystem.hpp>
#include <CombatAndroid/ECS/Utility/UI/GameMenu.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/ECS/Utility/Table/WeaponEvolutionTable.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Core/Window.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct SkillSelectParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/SkillSelect.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct SkillSelectParams {
            //-------------------------------------------------------------
            // カードの見た目のチューニング値（全て画面ピクセル単位）。
            // 位置はメニューを開くたびにウィンドウサイズから計算し直す。
            // 縦長のカードを横に扇状に並べ、選んだカードだけまっすぐ起こして持ち上げる
            //-------------------------------------------------------------
            float cardWidth = 340.0f;
            float cardHeight = 460.0f;
            float cardGap = 48.0f;                     //!< カード同士の横の隙間（カード1枚ぶんの送りは cardWidth + cardGap）
            float cardCenterOffsetY = 40.0f;           //!< 画面中心から見た、真ん中のカードの中心Y
            float fanRollDegrees = 6.0f;               //!< 中央から1枚離れるごとの傾き（度。右のカードほど時計回り）
            float fanDropY = 28.0f;                    //!< 中央から1枚離れるごとに下げる量
            float selectedLiftY = 36.0f;               //!< 選んだカードを持ち上げる量
            float selectedScale = 1.06f;               //!< 選んだカードの大きさ
            float unselectedScale = 0.94f;             //!< 選んでいないカードの大きさ
            float unselectedDim = 0.5f;                //!< 選んでいないカードの明るさ（色に掛ける）
            float accentOffsetX = 14.0f;               //!< 選んだカードの後ろに敷く差し色の板のずらしX（右下へ）
            float accentOffsetY = 14.0f;               //!< 同じくY
            float textInsetX = 24.0f;                  //!< カードの左右の端から文字までの余白（はみ出す文字は縮める）
            float nameOffsetY = 48.0f;                 //!< カード中心から見たスキル名のY
            float levelOffsetY = 106.0f;               //!< カード中心から見た段階（Lv）のY
            float descOffsetY = 160.0f;                //!< カード中心から見た説明文のY

            float titleOffsetX = -360.0f;              //!< 画面中心から見た「LEVEL UP!」のX（中央を外して左へ寄せる）
            float titleOffsetY = -310.0f;              //!< 画面中心から見た「LEVEL UP!」のY
            float bandHeight = 120.0f;                 //!< 見出しの後ろの斜めの帯の高さ（幅は画面からはみ出す長さ）
            float bandRollDegrees = -5.0f;             //!< 帯の傾き（度。負で右上がり）

            hlslpp::float4 backdropColor = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.65f);      //!< 画面全体の暗転
            hlslpp::float4 highlightColor = hlslpp::float4(0.78f, 0.06f, 0.1f, 1.0f);    //!< 選んだカードの後ろの差し色の板
            hlslpp::float4 bandColor = hlslpp::float4(0.78f, 0.06f, 0.1f, 1.0f);         //!< 見出しの帯
            hlslpp::float4 titleColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 nameColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 levelColor = hlslpp::float4(1.0f, 0.86f, 0.5f, 1.0f);
            hlslpp::float4 descColor = hlslpp::float4(0.92f, 0.92f, 0.92f, 1.0f);

            //-------------------------------------------------------------
            // レベルアップ後無敵（PlayerComponent::levelUpInvincibleTimer）の間、
            // プレイヤーモデルへ焼く発光のチューニング値。武器レベルアップの金色発光
            // （PickupSystem）と似た色味にしつつ、脈動させて「拾得」ではなく
            // 「無敵中」だと分かるようにしている
            //-------------------------------------------------------------
            float levelUpInvincibleFadeOutDuration = 0.5f;    //!< 無敵が切れる直前、この秒数かけて発光を落とす
            float levelUpInvinciblePulseSpeed = 6.0f;    //!< 脈動速度（rad/sec相当）
            hlslpp::float3 levelUpInvincibleRimColor = hlslpp::float3(1.0f, 0.95f, 0.75f);    //!< 金〜白系のリムカラー
            float levelUpInvincibleRimIntensityMax = 5.0f;
            float levelUpInvincibleRimPower = 2.5f;
            float levelUpInvincibleGlowMin = 0.15f;    //!< 白発光の脈動の下限
            float levelUpInvincibleGlowMax = 0.5f;     //!< 白発光の脈動の上限
            float confirmLockSeconds = 0.6f;    //!< 開いてから決定（F・クリック）を受け付けるまでの秒（実時間）。攻撃の連打で誤って決めないため
        };

        template <class Archive>
        void load(Archive& archive, SkillSelectParams& params) {
            LoadField(archive, "cardWidth", params.cardWidth);
            LoadField(archive, "cardHeight", params.cardHeight);
            LoadField(archive, "cardGap", params.cardGap);
            LoadField(archive, "cardCenterOffsetY", params.cardCenterOffsetY);
            LoadField(archive, "fanRollDegrees", params.fanRollDegrees);
            LoadField(archive, "fanDropY", params.fanDropY);
            LoadField(archive, "selectedLiftY", params.selectedLiftY);
            LoadField(archive, "selectedScale", params.selectedScale);
            LoadField(archive, "unselectedScale", params.unselectedScale);
            LoadField(archive, "unselectedDim", params.unselectedDim);
            LoadField(archive, "accentOffsetX", params.accentOffsetX);
            LoadField(archive, "accentOffsetY", params.accentOffsetY);
            LoadField(archive, "textInsetX", params.textInsetX);
            LoadField(archive, "nameOffsetY", params.nameOffsetY);
            LoadField(archive, "levelOffsetY", params.levelOffsetY);
            LoadField(archive, "descOffsetY", params.descOffsetY);
            LoadField(archive, "titleOffsetX", params.titleOffsetX);
            LoadField(archive, "titleOffsetY", params.titleOffsetY);
            LoadField(archive, "bandHeight", params.bandHeight);
            LoadField(archive, "bandRollDegrees", params.bandRollDegrees);
            LoadField(archive, "backdropColor", params.backdropColor);
            LoadField(archive, "highlightColor", params.highlightColor);
            LoadField(archive, "bandColor", params.bandColor);
            LoadField(archive, "titleColor", params.titleColor);
            LoadField(archive, "nameColor", params.nameColor);
            LoadField(archive, "levelColor", params.levelColor);
            LoadField(archive, "descColor", params.descColor);
            LoadField(archive, "levelUpInvincibleFadeOutDuration", params.levelUpInvincibleFadeOutDuration);
            LoadField(archive, "levelUpInvinciblePulseSpeed", params.levelUpInvinciblePulseSpeed);
            LoadField(archive, "levelUpInvincibleRimColor", params.levelUpInvincibleRimColor);
            LoadField(archive, "levelUpInvincibleRimIntensityMax", params.levelUpInvincibleRimIntensityMax);
            LoadField(archive, "levelUpInvincibleRimPower", params.levelUpInvincibleRimPower);
            LoadField(archive, "levelUpInvincibleGlowMin", params.levelUpInvincibleGlowMin);
            LoadField(archive, "levelUpInvincibleGlowMax", params.levelUpInvincibleGlowMax);
            LoadField(archive, "confirmLockSeconds", params.confirmLockSeconds);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const SkillSelectParams& GetParams() {
            static const SkillSelectParams s_params = LoadSystemParams<SkillSelectParams>("SkillSelect");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief 0から1を滑らかに補間する関数（smoothstepの本体部分）
        //-------------------------------------------------------------
        [[nodiscard]]
        float SmoothStep01(float t) {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        //-------------------------------------------------------------
        //! @brief レベルアップ後無敵のタイマーを実時間で減衰させ、プレイヤーの
        //!        RimGlowComponentへ発光値を書き込む。0を切ったら消灯する
        //-------------------------------------------------------------
        void TickLevelUpInvincibility(Tsukino::ECS::Registry& registry, entt::entity entity, PlayerComponent& player, float deltaTime) {
            const SkillSelectParams& params = GetParams();

            player.levelUpInvincibleTimer     = std::max(player.levelUpInvincibleTimer - deltaTime, 0.0f);
            player.levelUpInvinciblePulseTime += deltaTime;

            auto* rimGlow = registry.try_get<Tsukino::BuiltIn::ECS::RimGlowComponent>(entity);
            if(!rimGlow)
                return;

            if(player.levelUpInvincibleTimer <= 0.0f) {
                rimGlow->active = false;
                return;
            }

            // 無敵終了間際だけイーズアウトさせ、それ以外は1.0（フル発光）のまま
            float fadeEase = SmoothStep01(player.levelUpInvincibleTimer / params.levelUpInvincibleFadeOutDuration);

            float wave  = std::sin(player.levelUpInvinciblePulseTime * params.levelUpInvinciblePulseSpeed);
            float pulse = wave * wave;

            rimGlow->active       = true;
            rimGlow->rimColor     = params.levelUpInvincibleRimColor;
            rimGlow->rimIntensity = params.levelUpInvincibleRimIntensityMax * fadeEase;
            rimGlow->rimPower     = params.levelUpInvincibleRimPower;
            rimGlow->glow         = (params.levelUpInvincibleGlowMin + (params.levelUpInvincibleGlowMax - params.levelUpInvincibleGlowMin) * pulse) * fadeEase;
        }

        //-------------------------------------------------------------
        //! @brief  エンティティを非表示にする関数
        //! @param  registry [in] ECSレジストリ
        //! @param  entity   [in] 対象のエンティティ
        //! @note   SpriteRenderSystemは極小スケールを、FontRendererSystemは空文字を
        //!         それぞれ描画対象から外すため、この2つを書けば消える（HPバー等と同じ流儀）
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
        //! @struct CardPlacement
        //! @brief  カード1枚の置き方（中心・傾き・大きさ）
        //-------------------------------------------------------------
        struct CardPlacement {
            float x     = 0.0f;    //!< 中心のスクリーンX
            float y     = 0.0f;    //!< 中心のスクリーンY
            float roll  = 0.0f;    //!< 傾き（ラジアン。時計回りが正）
            float scale = 1.0f;    //!< 大きさ
        };

        //-------------------------------------------------------------
        //! @brief  index枚目のカードの置き方を求める関数
        //! @param  screenCenterX  [in] 画面中央のX
        //! @param  screenCenterY  [in] 画面中央のY
        //! @param  index          [in] 何枚目か（0始まり）
        //! @param  candidateCount [in] 今回並べるカードの枚数
        //! @param  selected       [in] 選ばれているカードか
        //! @return カードの置き方
        //! @note   枚数が1〜3のどれでも画面中央に対して対称に、扇を開いたように並ぶ。
        //!         中央から離れるほど傾けて下げ、選んだカードだけまっすぐ起こして持ち上げる
        //-------------------------------------------------------------
        [[nodiscard]]
        CardPlacement CalculateCardPlacement(float screenCenterX, float screenCenterY, int index, int candidateCount, bool selected) {
            const SkillSelectParams& params = GetParams();

            const float fromCenter = static_cast<float>(index) - static_cast<float>(candidateCount - 1) * 0.5f;

            CardPlacement placement;
            placement.x     = screenCenterX + fromCenter * (params.cardWidth + params.cardGap);
            placement.y     = screenCenterY + params.cardCenterOffsetY + std::abs(fromCenter) * params.fanDropY;
            placement.roll  = fromCenter * params.fanRollDegrees * std::numbers::pi_v<float> / 180.0f;
            placement.scale = params.unselectedScale;

            if(selected) {
                placement.y -= params.selectedLiftY;
                placement.roll  = 0.0f;
                placement.scale = params.selectedScale;
            }
            return placement;
        }

        //-------------------------------------------------------------
        //! @brief  カードの中心から見た位置（カードと一緒に傾ける）をスクリーン座標へ直す関数
        //! @param  card    [in] カードの置き方
        //! @param  offsetY [in] カード中心から下へ向かう距離（傾けて大きさを掛ける前）
        //! @param  x       [out] スクリーンX
        //! @param  y       [out] スクリーンY
        //! @note   文字は回せないので、位置だけカードの傾きに沿わせる
        //-------------------------------------------------------------
        void CardLocalToScreen(const CardPlacement& card, float offsetY, float& x, float& y) {
            const float scaled = offsetY * card.scale;
            x = card.x - scaled * std::sin(card.roll);
            y = card.y + scaled * std::cos(card.roll);
        }

        //-------------------------------------------------------------
        //! @brief  文字の描画幅の上限を書く関数（収まらない文字は縮めて収める）
        //-------------------------------------------------------------
        void SetTextMaxWidth(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float maxWidth) {
            if(auto* font = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity))
                font->maxWidth = maxWidth;
        }

        //-------------------------------------------------------------
        //! @brief  スキルカードに添える進化の案内を作る関数
        //! @param  registry [in] ECSレジストリ
        //! @param  player   [in] 手持ちの武器を持つプレイヤー
        //! @param  skill    [in] カードのスキル
        //! @return このスキルを条件に進化する、手持ちで未進化の武器の進化後の名前（「・」区切り）。無ければ空
        //-------------------------------------------------------------
        [[nodiscard]]
        std::wstring FindEvolutionHint(Tsukino::ECS::Registry& registry, const PlayerComponent& player, SkillId skill) {
            std::wstring hint;
            for(Tsukino::ECS::Entity weaponEntity : player.weaponInventory) {
                const auto* weapon = registry.try_get<WeaponComponent>(weaponEntity);
                if(!weapon || weapon->evolved)
                    continue;

                const WeaponEvolutionEntry& evolution = GetWeaponEvolution(weapon->weaponId);
                if(evolution.requiredSkill != skill)
                    continue;

                if(!hint.empty())
                    hint += L"・";
                hint += evolution.displayName;
            }
            return hint;
        }

        //-------------------------------------------------------------
        //! @brief  メニュー全体の見た目を書き直す関数
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  select   [in] メニューの状態
        //! @param  skills   [in] 取得済みスキル（「Lv.2 / 5」の表示に使う）
        //! @note   開いた時とカーソルが動いた時に呼ぶ。毎フレーム呼んでも問題はないが、
        //!         文字列を組み立てるので変化があった時だけにしている
        //-------------------------------------------------------------
        void RefreshUi(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, SkillSelectComponent& select,
                       const PlayerSkillComponent& skills, const PlayerComponent& player) {
            const SkillSelectParams& params = GetParams();

            const float screenWidth  = context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f;
            const float screenHeight  = context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;

            //-------------------------------------------------------------
            // 画面全体の暗転と、見出しを載せる斜めの帯。帯は画面の左右へはみ出させ、
            // 見出しは中央を外して左へ寄せる（全部を中央の縦軸に揃えると単調になる）
            //-------------------------------------------------------------
            StretchSprite(registry, context, select.backdropEntity, screenCenterX, screenCenterY, screenWidth, screenHeight, params.backdropColor);

            const float titleX = screenCenterX + params.titleOffsetX;
            const float titleY = screenCenterY + params.titleOffsetY;
            StretchSpriteRotated(registry, context, select.bandEntity, screenCenterX, titleY, screenWidth * 1.4f, params.bandHeight,
                                 params.bandRollDegrees * std::numbers::pi_v<float> / 180.0f, params.bandColor);
            PlaceUiText(registry, select.titleEntity, titleX, titleY, GetUiTextScale(UiTextSize::Display), L"LEVEL UP!", params.titleColor);

            const int cursorIndex = std::clamp(select.cursorIndex, 0, std::max(select.candidateCount - 1, 0));

            //-------------------------------------------------------------
            // カード本体
            //-------------------------------------------------------------
            for(int i = 0; i < kSkillChoiceMax; ++i) {
                SkillSelectCardEntities& card = select.cards[static_cast<size_t>(i)];

                if(i >= select.candidateCount) {
                    // カンストで選択肢が3つに満たない回。余ったカードは消しておく
                    HideEntity(registry, card.panelEntity);
                    HideEntity(registry, card.nameEntity);
                    HideEntity(registry, card.levelEntity);
                    HideEntity(registry, card.descEntity);
                    HideEntity(registry, card.hitEntity);
                    continue;
                }

                const SkillId          id        = select.candidates[static_cast<size_t>(i)];
                const SkillTableEntry& entry     = GetSkillEntry(id);
                const int              level     = skills.levels[static_cast<size_t>(id)];
                const bool             selected  = (i == cursorIndex);
                const CardPlacement    placement = CalculateCardPlacement(screenCenterX, screenCenterY, i, select.candidateCount, selected);
                const float            width     = params.cardWidth * placement.scale;
                const float            height    = params.cardHeight * placement.scale;

                //-------------------------------------------------------------
                // 背景。テクスチャはテーブルのパスから引く（AssetManagerがパスで
                // キャッシュするため、開くたびにLoadを呼んでも実際の読み込みは1回きり）
                //-------------------------------------------------------------
                Tsukino::Asset::AssetHandle cardTexture{};
                if(context.assetManager) {
                    cardTexture = context.assetManager->Load(Tsukino::Core::Path(entry.backgroundTexturePath));
                    if(auto* panelSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(card.panelEntity))
                        panelSprite->textureHandle = cardTexture;
                }

                // 非選択のカードは暗くして沈ませる
                const float          dim        = selected ? 1.0f : params.unselectedDim;
                const hlslpp::float4 panelColor = hlslpp::float4(entry.panelColor.x * dim, entry.panelColor.y * dim, entry.panelColor.z * dim, entry.panelColor.w);

                StretchSpriteRotated(registry, context, card.panelEntity, placement.x, placement.y, width, height, placement.roll, panelColor);

                //-------------------------------------------------------------
                // 選んだカードの後ろに、同じ形（カードの絵のアルファ）の差し色の板を右下へずらして敷く。
                // 四角い枠で囲むより、カードが浮き上がって見える
                //-------------------------------------------------------------
                if(selected) {
                    if(auto* highlightSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(select.highlightEntity))
                        highlightSprite->textureHandle = cardTexture;
                    StretchSpriteRotated(registry, context, select.highlightEntity, placement.x + params.accentOffsetX,
                                         placement.y + params.accentOffsetY, width, height, placement.roll, params.highlightColor);
                }

                // マウスの当たり判定は回らない矩形なので、傾いたカードからはみ出さないよう一回り小さくする
                StretchSprite(registry, context, card.hitEntity, placement.x, placement.y, width * 0.9f, height * 0.9f,
                              hlslpp::float4(0.0f, 0.0f, 0.0f, 0.0f));

                //-------------------------------------------------------------
                // 文言。levelは取得済みの段階数なので、今回取ると level+1 段階目になる。
                // 名前を大きく、段階と説明を小さくして、文字にも大小の差を付ける
                //-------------------------------------------------------------
                std::wstring levelText = L"Lv.";
                levelText += std::to_wstring(level + 1);
                levelText += L" / ";
                levelText += std::to_wstring(kMaxSkillLevel);

                // 持っている武器の進化条件になっているスキルなら、進化先を添えて気付かせる
                const std::wstring evolutionHint = FindEvolutionHint(registry, player, id);
                if(!evolutionHint.empty()) {
                    levelText += L"   進化: ";
                    levelText += evolutionHint;
                }

                const float textMaxWidth = (params.cardWidth - params.textInsetX * 2.0f) * placement.scale;
                const float textAlpha    = selected ? 1.0f : 0.7f;
                auto        fade         = [textAlpha](const hlslpp::float4& color) {
                    return hlslpp::float4(color.x, color.y, color.z, color.w * textAlpha);
                };

                float textX = 0.0f;
                float textY = 0.0f;
                CardLocalToScreen(placement, params.nameOffsetY, textX, textY);
                SetTextMaxWidth(registry, card.nameEntity, textMaxWidth);
                PlaceUiText(registry, card.nameEntity, textX, textY, GetUiTextScale(UiTextSize::Title) * placement.scale, entry.displayName,
                            fade(params.nameColor));

                CardLocalToScreen(placement, params.levelOffsetY, textX, textY);
                SetTextMaxWidth(registry, card.levelEntity, textMaxWidth);
                PlaceUiText(registry, card.levelEntity, textX, textY, GetUiTextScale(UiTextSize::Body) * placement.scale, levelText,
                            fade(params.levelColor));

                CardLocalToScreen(placement, params.descOffsetY, textX, textY);
                SetTextMaxWidth(registry, card.descEntity, textMaxWidth);
                PlaceUiText(registry, card.descEntity, textX, textY, GetUiTextScale(UiTextSize::Body) * placement.scale,
                            entry.levels[static_cast<size_t>(level)].description, fade(params.descColor));
            }
        }

        //-------------------------------------------------------------
        //! @brief  開いた瞬間の動き（暗転が浮かび、見出しがポップし、カードが上から順に横から滑り込む）
        //! @note   メニュー中はシーンがdeltaTime=0を渡すので、動きはどれも実時間で進める
        //-------------------------------------------------------------
        void StartOpenMotion(Tsukino::ECS::Registry& registry, SkillSelectComponent& select) {
            UiAppear(registry, select.backdropEntity, UiAppearDesc{.unscaledTime = true});
            UiAppear(registry, select.bandEntity, UiAppearDesc{.slideX = -240.0f, .unscaledTime = true});
            UiPopIn(registry, select.titleEntity, 0.08f, true);

            // カードは画面の下から1枚ずつ跳ね上がる
            constexpr float kCardSlideY  = 180.0f;
            constexpr float kCardStagger = 0.07f;
            for(int i = 0; i < select.candidateCount; ++i) {
                SkillSelectCardEntities& card = select.cards[static_cast<size_t>(i)];
                // カードの文字は板と一緒に動かす（一度付ければ以後ずっと追従する）
                UiFollow(registry, card.nameEntity, card.panelEntity);
                UiFollow(registry, card.levelEntity, card.panelEntity);
                UiFollow(registry, card.descEntity, card.panelEntity);
                UiAppear(registry, card.panelEntity,
                         UiAppearDesc{.slideY = kCardSlideY, .delay = kCardStagger * static_cast<float>(i + 1), .unscaledTime = true});
            }

            // 差し色の板は選んでいるカードと一緒に出し、ゆっくり呼吸させて「選べる」ことを見せる
            UiAppear(registry, select.highlightEntity,
                     UiAppearDesc{.slideY = kCardSlideY, .delay = kCardStagger * static_cast<float>(select.cursorIndex + 1), .unscaledTime = true});
            UiStopPulse(registry, select.highlightEntity);
            UiPulse(registry, select.highlightEntity, 0.35f, true);
        }

        //-------------------------------------------------------------
        //! @brief  カーソルが動いたときの動き（差し色の板が前のカードから滑って移り、選んだカードが軽く弾む）
        //-------------------------------------------------------------
        void StartCursorMotion(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, SkillSelectComponent& select,
                               int previousIndex) {
            const float screenCenterX = (context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f) * 0.5f;
            const float screenCenterY = (context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f) * 0.5f;

            const CardPlacement from = CalculateCardPlacement(screenCenterX, screenCenterY, previousIndex, select.candidateCount, true);
            const CardPlacement to   = CalculateCardPlacement(screenCenterX, screenCenterY, select.cursorIndex, select.candidateCount, true);

            UiSlideFrom(registry, select.highlightEntity, from.x - to.x, from.y - to.y, true);
            UiPunch(registry, select.cards[static_cast<size_t>(select.cursorIndex)].panelEntity, 0.4f, true);
        }

        //-------------------------------------------------------------
        //! @brief  メニューを構成するエンティティを全て消す関数
        //! @param  registry [in] ECSレジストリ
        //! @param  select   [in] メニューの状態
        //-------------------------------------------------------------
        void HideUi(Tsukino::ECS::Registry& registry, SkillSelectComponent& select) {
            HideEntity(registry, select.backdropEntity);
            HideEntity(registry, select.highlightEntity);
            HideEntity(registry, select.bandEntity);
            HideEntity(registry, select.titleEntity);
            UiStopPulse(registry, select.highlightEntity);    // 隠している間まで呼吸を回し続けない

            for(SkillSelectCardEntities& card : select.cards) {
                HideEntity(registry, card.panelEntity);
                HideEntity(registry, card.nameEntity);
                HideEntity(registry, card.levelEntity);
                HideEntity(registry, card.descEntity);
                HideEntity(registry, card.hitEntity);    // 閉じている間はマウスに反応させない
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 並べたカード全体の広がりを得る
    //-------------------------------------------------------------
    SkillSelectFanExtent GetSkillSelectFanExtent(Tsukino::ECS::Registry& registry) {
        const SkillSelectParams& params = GetParams();

        int candidateCount = 1;
        auto view = registry.View<SkillSelectComponent>();
        for(entt::entity entity : view) {
            candidateCount = std::max(view.get<SkillSelectComponent>(entity).candidateCount, 1);
            break;
        }

        SkillSelectFanExtent extent;
        extent.halfWidth     = (static_cast<float>(candidateCount) * params.cardWidth + static_cast<float>(candidateCount - 1) * params.cardGap) * 0.5f;
        extent.centerOffsetY = params.cardCenterOffsetY;
        extent.bottomOffsetY = params.cardCenterOffsetY + params.cardHeight * 0.5f
                               + static_cast<float>(candidateCount - 1) * 0.5f * params.fanDropY;
        return extent;
    }

    //-------------------------------------------------------------
    //! @brief 今スキル選択で進行を止めているかを問い合わせる
    //-------------------------------------------------------------
    bool IsSkillSelectActive(Tsukino::ECS::Registry& registry) {
        auto view = registry.View<SkillSelectComponent>();
        for(entt::entity entity : view) {
            const SkillSelectComponent& select = view.get<SkillSelectComponent>(entity);
            // ムービー中に溜まったレベルアップは、ムービーが終わってから開く（それまでは止めない）
            if(!select.isActive && select.closingBlockFrames <= 0 && IsCutsceneActive(registry))
                return false;
            return select.isActive || select.pendingLevelUps > 0 || select.closingBlockFrames > 0;
        }
        return false;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void SkillSelectSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        // メニューの動きはUiMotion（実時間）に任せるため、表示中はdeltaTimeを使わない
        //（そもそも表示中はシーンがdeltaTime=0を渡してくる）。メニューが閉じた後の
        // レベルアップ後無敵演出（下記）だけは実時間で進行させるため、ここでは捨てない

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        auto view = registry.View<PlayerComponent, SkillSelectComponent, PlayerSkillComponent>();
        for(entt::entity entity : view) {
            auto& player = view.get<PlayerComponent>(entity);
            auto& select = view.get<SkillSelectComponent>(entity);
            auto& skills = view.get<PlayerSkillComponent>(entity);

            if(!select.isActive && select.pendingLevelUps <= 0) {
                //-------------------------------------------------------------
                // レベルアップ後無敵の消化とリムグロー。全てのスキル選択が終わり切った
                // 瞬間（下の決定処理でlevelUpInvincibleTimerを立てる）から実時間で
                // 減衰させる。既存の溜め攻撃リムライト（PlayerAnimationSystem）と
                // 同じRimGlowComponentを流用するが、あちらは溜め中しか書き込まないため
                // 通常時はこちらの書き込みがそのまま残る
                //-------------------------------------------------------------
                if(player.levelUpInvincibleTimer > 0.0f)
                    TickLevelUpInvincibility(registry, entity, player, deltaTime);

                //-------------------------------------------------------------
                // 決定した次のフレーム。この1フレームだけ停止を延長して、
                // 決定に使ったFの押し込みが完全に流れきるのを待つ
                // （IsKeyPressedは立ち上がり検出なので、1フレーム挟めば消える。
                //   これを怠るとPickupSystemが同じF入力を拾得として拾ってしまう）
                //-------------------------------------------------------------
                if(select.closingBlockFrames > 0) {
                    --select.closingBlockFrames;
                    SuppressAllMoveInput(registry);
                }
                continue;    // 平常時。何もしない
            }

            select.closingBlockFrames = 0;

            //-------------------------------------------------------------
            // 走行が終わった（死亡・クリア）後はメニューを出さない。
            // 終わりと同じフレームにレベルアップが積まれることがあり、リザルトと同じF・W/Sを
            // 取り合ってしまうため、残っている予約ごと捨てる
            //-------------------------------------------------------------
            if(IsRunEnded(registry)) {
                select.pendingLevelUps = 0;
                select.isActive        = false;
                HideUi(registry, select);
                continue;
            }

            // ムービー（ライブのカットシーン）の間は開かず、予約だけ残しておく
            if(!select.isActive && IsCutsceneActive(registry))
                continue;

            // 停止中はプレイヤーも敵も移動入力を持たない状態に固定する
            SuppressAllMoveInput(registry);

            //-------------------------------------------------------------
            // メニューを開く
            //-------------------------------------------------------------
            if(!select.isActive) {
                const int candidateCount = PickSkillCandidates(m_rng, skills.levels, select.candidates);
                if(candidateCount <= 0) {
                    // 全スキルがカンスト済み。出す物が無いので予約ごと捨て、そのままゲームを続ける
                    select.pendingLevelUps = 0;
                    continue;
                }

                select.candidateCount  = candidateCount;
                select.cursorIndex     = 0;
                select.isActive         = true;
                select.openedThisFrame  = true;
                select.confirmLockTimer = GetParams().confirmLockSeconds;

                //-------------------------------------------------------------
                // 進行中のヒットストップを打ち切る。CombatAndroidScene::OnUpdateは
                // メニュー中はdeltaTimeを0で上書きするためHitStopSystemの減算処理を
                // 通らず、放っておくとメニューを閉じた後にスローモーションが残ってしまう
                //-------------------------------------------------------------
                ClearAllHitStop(registry);

                RefreshUi(registry, *ctx, select, skills, player);
                StartOpenMotion(registry, select);
                continue;    // 表示した直後のフレームでそのまま決定入力を拾わない
            }

            if(select.openedThisFrame) {
                select.openedThisFrame = false;
                continue;
            }

            // 決定の受付待ち。メニュー中はdeltaTimeが0なので実時間で減らす
            if(select.confirmLockTimer > 0.0f) {
                const float realDeltaTime =
                    registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : 0.0f;
                select.confirmLockTimer = std::max(select.confirmLockTimer - realDeltaTime, 0.0f);
            }

            if(!ctx->inputSystem)
                continue;

            //-------------------------------------------------------------
            // カーソルの左右移動。A/D・矢印キー・W/S・マウスホイールを同じ「1枚ぶん」として扱う
            //（ホイールは1ノッチ=±1.0。上へ回す＝左のカードへ＝インデックスは減る）
            //-------------------------------------------------------------
            const Tsukino::Input::InputSystem& input = *ctx->inputSystem;
            int step = 0;
            if(input.IsKeyPressed(Tsukino::Input::KeyCode::A) || input.IsKeyPressed(Tsukino::Input::KeyCode::Left)
               || input.IsKeyPressed(Tsukino::Input::KeyCode::W))
                --step;
            if(input.IsKeyPressed(Tsukino::Input::KeyCode::D) || input.IsKeyPressed(Tsukino::Input::KeyCode::Right)
               || input.IsKeyPressed(Tsukino::Input::KeyCode::S))
                ++step;

            const float wheelDelta = input.GetWheelDelta();
            if(wheelDelta > 0.0f)
                --step;
            else if(wheelDelta < 0.0f)
                ++step;
            step = std::clamp(step, -1, 1);

            //-------------------------------------------------------------
            // マウス。カードに乗せたらそのカードへ合わせ、クリックで決定する。
            // 開いた最初のフレームは上（openedThisFrame）で捨てており、クリックは押した瞬間しか
            // 立たないので、戦闘中に押していた攻撃のクリックで勝手に選ばれることはない
            //-------------------------------------------------------------
            std::array<Tsukino::ECS::Entity, kSkillChoiceMax> cardHits{};
            for(int i = 0; i < kSkillChoiceMax; ++i)
                cardHits[static_cast<size_t>(i)] = select.cards[static_cast<size_t>(i)].hitEntity;
            const GameMenuPointer pointer =
                ReadPointerOverRows(registry, *ctx->inputSystem, std::span<const Tsukino::ECS::Entity>(cardHits.data(), select.candidateCount));

            if(step != 0 || pointer.hoverIndex >= 0) {
                const int nextIndex = pointer.hoverIndex >= 0 ? pointer.hoverIndex
                                                              : std::clamp(select.cursorIndex + step, 0, select.candidateCount - 1);
                if(nextIndex != select.cursorIndex) {
                    const int previousIndex = select.cursorIndex;
                    select.cursorIndex      = nextIndex;
                    PlaySound(registry, SoundId::MenuMove);
                    RefreshUi(registry, *ctx, select, skills, player);
                    StartCursorMotion(registry, *ctx, select, previousIndex);
                }
            }

            //-------------------------------------------------------------
            // 決定。取得段階を1つ進めて実効値を計算し直し、メニューを閉じる。
            // pendingLevelUpsがまだ残っていれば次のフレームで2回目のメニューが開く
            //（IsSkillSelectActiveはpendingLevelUpsも見ているので停止は続いたまま）
            //-------------------------------------------------------------
            const bool confirmUnlocked = select.confirmLockTimer <= 0.0f;
            if(confirmUnlocked && (ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F) || pointer.clicked)) {
                const SkillId acquiredId = select.candidates[static_cast<size_t>(select.cursorIndex)];

                int& acquiredLevel = skills.levels[static_cast<size_t>(acquiredId)];
                acquiredLevel      = std::min(acquiredLevel + 1, kMaxSkillLevel);
                skills.lastAcquired = acquiredId;
                RecalculateSkillStats(skills);

                //-------------------------------------------------------------
                // 画面右の取得ログへ流す。メニューを閉じるまではdeltaTimeが0のため
                // 演出は止まったままだが、ログは暗転板より奥の層に居て見えないので、
                // 実際にはメニューが閉じた瞬間からスライドインが始まる
                //-------------------------------------------------------------
                if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>()) {
                    eventBus->Publish(GameLogEvent{GameLogCategory::SkillAcquired,
                                                  std::wstring(GetSkillEntry(acquiredId).displayName) + L" Lv."
                                                      + std::to_wstring(acquiredLevel)});
                }

                // スキルが条件に届いた武器を進化させる（ログはスキル取得の次の行に流れる）
                TryEvolvePlayerWeapons(registry, entity);

                --select.pendingLevelUps;
                select.isActive = false;
                HideUi(registry, select);

                //-------------------------------------------------------------
                // 予約されていたレベルアップを全て消化した（＝メニューがこの後
                // 続けて開き直らない）タイミングでのみ、レベルアップ後無敵を開始する。
                // 玉1個で複数レベル上がった場合は最後の1回の決定でだけ発生する
                //-------------------------------------------------------------
                if(select.pendingLevelUps <= 0) {
                    player.levelUpInvincibleTimer = player.levelUpInvincibleDuration;
                    player.levelUpInvinciblePulseTime = 0.0f;
                }

                // 次のレベルアップが残っていない場合、このフレームの後半でPlayerSystemが
                // 走ってしまうため、あと1フレームだけ停止を延長する
                select.closingBlockFrames = 1;

                //-------------------------------------------------------------
                // メニューへ入る前に積まれていた先行入力を捨てる。残したままだと
                // 閉じた次のフレームにPlayerAnimationSystemがそれを消費し、
                // 攻撃や回避が意図せず暴発する
                //-------------------------------------------------------------
                player.attackInputPressed = false;
                player.dodgeInputPressed  = false;
            }
        }
    }
}    // namespace CombatAndroid::ECS
