//-------------------------------------------------------------
//! @file   GameLogSystem.cpp
//! @brief  GameLogSystemクラスの実装
//! @note   時間はCombatAndroidScene::OnUpdateがヒットストップ／スキル選択用に
//!         スケールしたdeltaTimeをそのまま受け取る。そのためスキル選択メニュー表示中
//!         （deltaTime=0）はログの演出も止まるが、ログはモーダルの暗転板（400番台）より
//!         奥の層（220〜230）に居て見えないため、メニューが閉じた瞬間から
//!         スライドインが始まる形になり、演出としてはむしろ都合が良い
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/GameLogSystem.hpp>
#include <CombatAndroid/ECS/Component/GameLogComponent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>

#include <Tsukino/Core/Window.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <memory>
#include <utility>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 種別ごとの1行目の文言。並びはGameLogCategoryの定義順と一致していなければならない（下のstatic_assert）。
        // 色はチューニング値（GameLogParams::accentColors）の側に持つ
        //-------------------------------------------------------------
        const wchar_t* const kGameLogLabels[] = {
            L"武器を取得",
            L"武器レベルアップ",
            L"レベルアップ",
            L"スキル取得",
            L"危険度上昇",
            L"武器進化",
            L"エリート出現",
        };

        //! JSONで種別の色を引くときのキー。並びはkGameLogLabelsと同じ
        const char* const kGameLogCategoryKeys[] = {
            "WeaponAcquired", "WeaponLevelUp", "PlayerLevelUp", "SkillAcquired", "DangerRankUp", "WeaponEvolved", "EliteAppeared",
        };

        // 種別を足したのに表へ書き忘れる事故を防ぐ
        static_assert(std::size(kGameLogLabels) == static_cast<size_t>(GameLogCategory::Count),
                      "GameLogCategory に種別を足したら kGameLogLabels にも1行足すこと");
        static_assert(std::size(kGameLogCategoryKeys) == static_cast<size_t>(GameLogCategory::Count),
                      "GameLogCategory に種別を足したら kGameLogCategoryKeys にも1行足すこと");

        //-------------------------------------------------------------
        //! @struct GameLogParams
        //! @brief  見た目と演出のチューニング値（Assets/Tables/Systems/GameLog.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct GameLogParams {
            // 種別ごとの色。左端のバーと1行目の文字色で共有する（並びはGameLogCategory）
            std::array<hlslpp::float4, static_cast<size_t>(GameLogCategory::Count)> accentColors = {
                hlslpp::float4(1.00f, 0.82f, 0.35f, 1.0f),    // 武器を取得：金
                hlslpp::float4(1.00f, 0.60f, 0.25f, 1.0f),    // 武器レベルアップ：橙
                hlslpp::float4(0.40f, 0.80f, 1.00f, 1.0f),    // レベルアップ：水色
                hlslpp::float4(0.75f, 0.55f, 1.00f, 1.0f),    // スキル取得：紫
                hlslpp::float4(0.95f, 0.30f, 0.30f, 1.0f),    // 危険度上昇：赤
                hlslpp::float4(1.00f, 0.92f, 0.55f, 1.0f),    // 武器進化：明るい金
                hlslpp::float4(0.85f, 0.35f, 1.00f, 1.0f),    // エリート出現：紫（エリートの発光色）
            };

            //-------------------------------------------------------------
            // レイアウト（全て画面ピクセル単位）。位置は画面右端・画面高からの相対で
            // 毎フレーム求め直すため、ウィンドウサイズが変わっても崩れない
            //-------------------------------------------------------------
            float panelWidth  = 380.0f;
            float panelHeight = 68.0f;
            float slotGap     = 8.0f;     //!< 段同士の隙間（1段ぶんの送りは panelHeight + slotGap）
            float marginRight = 28.0f;    //!< 画面右端からパネル右端までの余白
            float bottomRatio = 0.62f;    //!< 最新の段（一番下）の下端＝画面高×これ

            float accentWidth  = 4.0f;     //!< 左端の種別色バーの幅
            float accentInsetX = 3.0f;     //!< パネル左端からバーまでの余白
            float accentInsetY = 10.0f;    //!< バーがパネル上下から内側へ引っ込む量

            float textInsetX   = 22.0f;     //!< パネル左端から文字までの余白
            float labelOffsetY = -16.0f;    //!< パネル中心から見た1行目（種別ラベル）のY
            float textOffsetY  = 15.0f;     //!< パネル中心から見た2行目（主題）のY

            // FontRendererSystemはworldMatrixのX軸長（＝scale.x）を拡大率として読む。
            // 基底のラスタライズサイズは32px（Default.dfont）なので、0.55で約18px、0.95で約30px
            float labelFontScale = 0.55f;
            float textFontScale  = 0.95f;

            float outlineWidth = 2.0f;    //!< 縁取りの太さ（ピクセル）

            //! 黒い半透明パネルと、2行目（主題）の文字色
            hlslpp::float4 panelColor   = hlslpp::float4(0.02f, 0.02f, 0.03f, 0.72f);
            hlslpp::float4 textColor    = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 outlineColor = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);

            //-------------------------------------------------------------
            // 演出（秒・ピクセル）
            //-------------------------------------------------------------
            float enterDuration    = 0.32f;     //!< 右からスライドインし切るまでの時間
            float lifetime         = 4.2f;      //!< 表示開始から消えるまでの総時間
            float exitDuration     = 0.55f;     //!< 寿命の最後のこの区間で上へフェードする
            float slideInDistance  = 120.0f;    //!< スライドインを始める位置（定位置から右へこの分ずらす）
            float exitRiseDistance = 34.0f;     //!< 消えるときに上へ動くピクセル数
            float slotLerpSpeed    = 14.0f;     //!< 段が繰り上がるときの追従の速さ
        };

        template <class Archive>
        void load(Archive& archive, GameLogParams& params) {
            // 種別の色は { "WeaponAcquired": {x,y,z,w}, ... } の形で読む。欠けた種別は既定値のまま
            try {
                archive.setNextName("accentColors");
                archive.startNode();
                for(size_t i = 0; i < params.accentColors.size(); ++i)
                    LoadField(archive, kGameLogCategoryKeys[i], params.accentColors[i]);
                archive.finishNode();
            } catch(const cereal::Exception&) {
                // accentColors自体が無い：既定値のまま
            }

            LoadField(archive, "panelWidth", params.panelWidth);
            LoadField(archive, "panelHeight", params.panelHeight);
            LoadField(archive, "slotGap", params.slotGap);
            LoadField(archive, "marginRight", params.marginRight);
            LoadField(archive, "bottomRatio", params.bottomRatio);
            LoadField(archive, "accentWidth", params.accentWidth);
            LoadField(archive, "accentInsetX", params.accentInsetX);
            LoadField(archive, "accentInsetY", params.accentInsetY);
            LoadField(archive, "textInsetX", params.textInsetX);
            LoadField(archive, "labelOffsetY", params.labelOffsetY);
            LoadField(archive, "textOffsetY", params.textOffsetY);
            LoadField(archive, "labelFontScale", params.labelFontScale);
            LoadField(archive, "textFontScale", params.textFontScale);
            LoadField(archive, "outlineWidth", params.outlineWidth);
            LoadField(archive, "panelColor", params.panelColor);
            LoadField(archive, "textColor", params.textColor);
            LoadField(archive, "outlineColor", params.outlineColor);
            LoadField(archive, "enterDuration", params.enterDuration);
            LoadField(archive, "lifetime", params.lifetime);
            LoadField(archive, "exitDuration", params.exitDuration);
            LoadField(archive, "slideInDistance", params.slideInDistance);
            LoadField(archive, "exitRiseDistance", params.exitRiseDistance);
            LoadField(archive, "slotLerpSpeed", params.slotLerpSpeed);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const GameLogParams& GetParams() {
            static const GameLogParams s_params = LoadSystemParams<GameLogParams>("GameLog");
            return s_params;
        }

        //! ウィンドウが取れなかったときのフォールバック（他の画面固定UIと同じ割り切り）
        constexpr float kFallbackScreenWidth  = 1700.0f;
        constexpr float kFallbackScreenHeight = 1000.0f;

        //-------------------------------------------------------------
        //! @brief 0から1を滑らかに補間する関数（smoothstepの本体部分）
        //-------------------------------------------------------------
        [[nodiscard]]
        float SmoothStep01(float t) {
            return t * t * (3.0f - 2.0f * t);
        }

        //-------------------------------------------------------------
        //! @brief  終わりに向かって減速するイージング（3次）
        //! @param  t [in] 0から1の進行度
        //-------------------------------------------------------------
        [[nodiscard]]
        float EaseOutCubic(float t) {
            float inverse = 1.0f - t;
            return 1.0f - inverse * inverse * inverse;
        }

        //-------------------------------------------------------------
        //! @brief  色のアルファへ更に係数を掛ける関数
        //! @param  color [in] 元の色
        //! @param  alpha [in] 掛ける係数（0〜1）
        //-------------------------------------------------------------
        [[nodiscard]]
        hlslpp::float4 WithAlpha(const hlslpp::float4& color, float alpha) {
            return hlslpp::float4(color.x, color.y, color.z, color.w * alpha);
        }

        //-------------------------------------------------------------
        //! @brief  種別を表の添字にする関数
        //! @param  category [in] 引きたい種別
        //! @note   範囲外（Count以上）はテーブル先頭へ丸める
        //-------------------------------------------------------------
        [[nodiscard]]
        size_t GetStyleIndex(GameLogCategory category) {
            size_t index = static_cast<size_t>(category);
            if(index >= std::size(kGameLogLabels))
                index = 0;

            return index;
        }

        //-------------------------------------------------------------
        //! @brief  スプライトを指定のピクセル矩形いっぱいに広げる関数
        //! @param  registry  [in] ECSレジストリ
        //! @param  context   [in] エンジンコンテキスト（テクスチャの実寸を引くのに使う）
        //! @param  entity    [in] 対象のエンティティ
        //! @param  centerX   [in] 矩形中心のスクリーンX
        //! @param  centerY   [in] 矩形中心のスクリーンY
        //! @param  width     [in] 矩形の幅（ピクセル）
        //! @param  height    [in] 矩形の高さ（ピクセル）
        //! @param  tintColor [in] スプライトに乗算する色
        //! @note   SpriteRenderSystemは「テクスチャの実ピクセル数 × transform.scale」を
        //!         最終的な大きさとして使うため、テクスチャサイズで割った値をscaleへ入れる
        //!         （SkillSelectSystemのStretchSpriteと同じ考え方）
        //-------------------------------------------------------------
        void StretchSprite(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, Tsukino::ECS::Entity entity,
                           float centerX, float centerY, float width, float height, const hlslpp::float4& tintColor) {
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
            transform->scale    = hlslpp::float3(width / textureWidth, height / textureHeight, 1.0f);
            transform->dirty    = true;

            sprite->tintColor = tintColor;
        }

        //-------------------------------------------------------------
        //! @brief  文字エンティティの位置・大きさ・色を書く関数
        //! @param  registry  [in] ECSレジストリ
        //! @param  entity    [in] 対象のエンティティ
        //! @param  x         [in] スクリーンX（文字列の左端）
        //! @param  y         [in] スクリーンY（文字列の中央）
        //! @param  fontScale [in] フォントの拡大率
        //! @param  baseColor [in] フェード前の文字色
        //! @param  alpha     [in] フェード係数（0〜1）
        //! @note   文言（FontComponent::text）はスロット割り当て時に一度だけ書くため、
        //!         ここでは触らない。縁取りだけが残らないよう本体と同じalphaを掛ける
        //-------------------------------------------------------------
        void PlaceLogText(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float x, float y, float fontScale,
                          const hlslpp::float4& baseColor, float alpha) {
            if(entity == entt::null)
                return;

            auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            auto* font      = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity);
            if(!transform || !font)
                return;

            transform->position = hlslpp::float3(x, y, 0.0f);
            transform->scale    = hlslpp::float3(fontScale, fontScale, 1.0f);
            transform->dirty    = true;

            const GameLogParams& params = GetParams();

            font->color        = WithAlpha(baseColor, alpha);
            font->outlineColor = WithAlpha(params.outlineColor, alpha);
            font->outlineWidth = params.outlineWidth;
        }

        //-------------------------------------------------------------
        //! @brief  スロットを構成する4エンティティをまとめて隠す関数
        //! @param  registry    [in] ECSレジストリ
        //! @param  panelEntity [in] スロットの根（パネル）
        //! @param  log         [in] そのスロットの状態
        //! @note   SpriteRenderSystemは極小スケールを、FontRendererSystemは空文字を
        //!         それぞれ描画対象から外すため、この2つを書けば消える
        //!         （SkillSelectSystemのHideEntityと同じ流儀）
        //-------------------------------------------------------------
        void HideSlot(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity panelEntity, const GameLogComponent& log) {
            const Tsukino::ECS::Entity spriteEntities[] = {panelEntity, log.accentEntity};
            for(Tsukino::ECS::Entity entity : spriteEntities) {
                if(entity == entt::null)
                    continue;

                if(auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity)) {
                    transform->scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
                    transform->dirty = true;
                }
            }

            const Tsukino::ECS::Entity fontEntities[] = {log.labelEntity, log.textEntity};
            for(Tsukino::ECS::Entity entity : fontEntities) {
                if(entity == entt::null)
                    continue;

                if(auto* font = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity))
                    font->text.clear();
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief GameLogEventの購読を開始する
    //-------------------------------------------------------------
    void GameLogSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        // ハンドラ内でヒープを触る回数を減らすため、あらかじめ最大数分を確保しておく
        m_pending.reserve(kGameLogPoolSize);
        (void)GetParams();    // 初回フレームでファイルを読まないよう先に読んでおく

        m_logConnection = eventBus.Subscribe<GameLogEvent>([this](const GameLogEvent& event) { OnGameLog(event); });
    }

    //-------------------------------------------------------------
    //! @brief 取得ログ通知のハンドラ
    //-------------------------------------------------------------
    void GameLogSystem::OnGameLog(const GameLogEvent& event) {
        m_pending.push_back(event);
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void GameLogSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        const GameLogParams& params = GetParams();

        float screenWidth  = ctx->window ? static_cast<float>(ctx->window->GetWidth()) : kFallbackScreenWidth;
        float screenHeight = ctx->window ? static_cast<float>(ctx->window->GetHeight()) : kFallbackScreenHeight;

        auto view = registry.View<GameLogComponent, Tsukino::BuiltIn::ECS::TransformComponent, Tsukino::BuiltIn::ECS::SpriteComponent>();

        //-------------------------------------------------------------
        // 保留中のログをスロットへ割り当てる
        //-------------------------------------------------------------
        for(const GameLogEvent& pending : m_pending) {
            //-------------------------------------------------------------
            // 空きスロットを探す。全て使用中なら最も古いもの（elapsedが最大）を再利用する
            //-------------------------------------------------------------
            entt::entity slotEntity = entt::null;
            float        oldest     = -1.0f;

            for(entt::entity entity : view) {
                const GameLogComponent& log = view.get<GameLogComponent>(entity);

                if(!log.active) {
                    slotEntity = entity;
                    break;
                }

                if(log.elapsed > oldest) {
                    oldest     = log.elapsed;
                    slotEntity = entity;
                }
            }

            if(slotEntity == entt::null)
                continue;    // プールが1つも用意されていない（シーン側の生成漏れ）

            const size_t styleIndex = GetStyleIndex(pending.category);

            GameLogComponent& log = view.get<GameLogComponent>(slotEntity);
            log.active            = true;
            log.elapsed           = 0.0f;
            log.sequence          = ++m_nextSequence;
            log.slotYInitialized  = false;    // 最初の1回だけ目標の段へ直接置く（下から生えさせない）
            log.accentColor       = params.accentColors[styleIndex];

            //-------------------------------------------------------------
            // 文言はここで一度だけ書き込む。以後Updateは位置と色だけを触る
            //-------------------------------------------------------------
            if(auto* labelFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(log.labelEntity))
                labelFont->text = kGameLogLabels[styleIndex];
            if(auto* textFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(log.textEntity))
                textFont->text = pending.subject;
        }

        m_pending.clear();

        //-------------------------------------------------------------
        // 表示中のスロットを新しい順（sequenceの降順）に並べ、段の番号を振り直す。
        // 0番＝一番下＝最新。プールは高々kGameLogPoolSize個なので固定長配列で足り、
        // 毎フレームのヒープ確保が要らない
        //-------------------------------------------------------------
        std::array<std::pair<unsigned int, entt::entity>, kGameLogPoolSize> ordered{};
        int                                                                 activeCount = 0;

        for(entt::entity entity : view) {
            const GameLogComponent& log = view.get<GameLogComponent>(entity);
            if(!log.active)
                continue;
            if(activeCount >= kGameLogPoolSize)
                break;

            ordered[static_cast<size_t>(activeCount)] = {log.sequence, entity};
            ++activeCount;
        }

        std::sort(ordered.begin(), ordered.begin() + activeCount,
                  [](const std::pair<unsigned int, entt::entity>& lhs, const std::pair<unsigned int, entt::entity>& rhs) {
                      return lhs.first > rhs.first;
                  });

        //-------------------------------------------------------------
        // 各段を進める
        //-------------------------------------------------------------
        float slotLerpT = 1.0f - std::exp(-params.slotLerpSpeed * deltaTime);

        for(int slotIndex = 0; slotIndex < activeCount; ++slotIndex) {
            entt::entity      panelEntity = ordered[static_cast<size_t>(slotIndex)].second;
            GameLogComponent& log         = view.get<GameLogComponent>(panelEntity);

            log.elapsed += deltaTime;

            //-------------------------------------------------------------
            // 寿命切れ。寿命は全段で共通なので、消えるのは常に一番上（最古）の段になり、
            // 下の段の番号がずれることは無い
            //-------------------------------------------------------------
            if(log.elapsed >= params.lifetime) {
                log.active = false;
                HideSlot(registry, panelEntity, log);
                continue;
            }

            //-------------------------------------------------------------
            // 段のY。新しい行が入って段が繰り上がるときも瞬間移動させず、
            // 目標へ指数減衰で滑らかに寄せる
            //-------------------------------------------------------------
            float targetY = screenHeight * params.bottomRatio - static_cast<float>(slotIndex) * (params.panelHeight + params.slotGap) - params.panelHeight * 0.5f;
            if(!log.slotYInitialized) {
                log.slotY            = targetY;
                log.slotYInitialized = true;
            } else {
                log.slotY += (targetY - log.slotY) * slotLerpT;
            }

            //-------------------------------------------------------------
            // 出入りの演出。
            // 入り：右からスライドイン（減速）しながらフェードイン
            // 出　：上へ持ち上げながらフェードアウト
            //       （スクリーン座標は下方向が正なので、上へ動かすにはYを負にする）
            //-------------------------------------------------------------
            float enterT = std::clamp(log.elapsed / params.enterDuration, 0.0f, 1.0f);
            float exitT  = std::clamp((log.elapsed - (params.lifetime - params.exitDuration)) / params.exitDuration, 0.0f, 1.0f);

            float offsetX = params.slideInDistance * (1.0f - EaseOutCubic(enterT));
            float offsetY = -params.exitRiseDistance * SmoothStep01(exitT);
            float alpha   = SmoothStep01(enterT) * (1.0f - SmoothStep01(exitT));

            float centerX   = screenWidth - params.marginRight - params.panelWidth * 0.5f + offsetX;
            float centerY   = log.slotY + offsetY;
            float panelLeft = centerX - params.panelWidth * 0.5f;

            //-------------------------------------------------------------
            // 見た目の反映。パネル・アクセントバー・種別ラベル・主題の4つへ
            // 同じalphaを掛け、1行がひとかたまりに見えるようにする
            //-------------------------------------------------------------
            StretchSprite(registry, *ctx, panelEntity, centerX, centerY, params.panelWidth, params.panelHeight, WithAlpha(params.panelColor, alpha));

            StretchSprite(registry, *ctx, log.accentEntity, panelLeft + params.accentInsetX + params.accentWidth * 0.5f, centerY, params.accentWidth,
                          params.panelHeight - params.accentInsetY * 2.0f, WithAlpha(log.accentColor, alpha));

            PlaceLogText(registry, log.labelEntity, panelLeft + params.textInsetX, centerY + params.labelOffsetY, params.labelFontScale, log.accentColor,
                         alpha);

            PlaceLogText(registry, log.textEntity, panelLeft + params.textInsetX, centerY + params.textOffsetY, params.textFontScale, params.textColor, alpha);
        }
    }
}    // namespace CombatAndroid::ECS
