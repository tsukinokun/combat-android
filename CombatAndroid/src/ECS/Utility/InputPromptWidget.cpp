//-------------------------------------------------------------
//! @file   InputPromptWidget.cpp
//! @brief  操作を促すUI（キーキャップ・マウス・矢印・長押しゲージ）の組み立てと更新の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/InputPromptWidget.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/Core/Path.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 部品の寸法（全て画面ピクセル単位。InputPromptStyle::scaleで一律に拡縮する）。
        // 実機で見ながら調整する値なので、意味のある名前を付けて1箇所に集めてある
        //-------------------------------------------------------------
        constexpr float kCapHeight       = 30.0f;    //!< キーキャップの高さ
        constexpr float kCapMinWidth     = 30.0f;    //!< キーキャップの最小幅（1文字ぶん。正方形に見せる）
        constexpr float kCapWidthPerChar = 15.0f;    //!< 文字数に応じて広げる量（"SPACE"のような複数文字キー用）
        constexpr float kCapBorder       = 3.0f;     //!< 外枠が面からはみ出す量（この2倍だけ外枠が大きい）
        constexpr float kGlyphScale      = 0.34f;    //!< キーの文字の拡大率（TransformComponent::scale.xがそのまま文字サイズ）

        constexpr float kMouseWidth        = 24.0f;    //!< マウス本体の幅
        constexpr float kMouseHeight       = 34.0f;    //!< マウス本体の高さ
        constexpr float kMouseButtonWidth  = 9.0f;     //!< 左ボタンの幅
        constexpr float kMouseButtonHeight = 14.0f;    //!< 左ボタンの高さ
        constexpr float kMouseButtonInset  = 2.5f;     //!< 左ボタンを本体の左上角から内側へ入れる量

        constexpr float kChevronArmLength = 14.0f;    //!< 矢印の腕1本の長さ
        constexpr float kChevronArmWidth  = 4.0f;     //!< 矢印の腕1本の太さ
        constexpr float kChevronGap       = 12.0f;    //!< キーキャップと矢印の間隔
        constexpr float kChevronArmSpread = 4.5f;     //!< 2本の腕をハの字に開くときの中心からのずらし量

        constexpr float kCaptionGap   = 14.0f;    //!< キーキャップと下のテキストの間隔
        constexpr float kCaptionScale = 0.30f;    //!< 下のテキストの拡大率

        constexpr float kRingRadius        = 34.0f;    //!< 長押しゲージの半径
        constexpr float kRingSegmentLength = 10.0f;    //!< セグメント1枚の長さ（半径方向）
        constexpr float kRingSegmentWidth  = 5.0f;     //!< セグメント1枚の太さ（円周方向）

        //-------------------------------------------------------------
        // 色。キーキャップは「明るい面に濃い文字」にして、暗い背景でも明るい背景でも
        // 輪郭が潰れないようにする（外枠の濃い色が明るい背景に対する縁取りになる）
        //-------------------------------------------------------------
        const hlslpp::float4 kCapBorderColor = hlslpp::float4(0.04f, 0.04f, 0.06f, 0.90f);    //!< キーキャップの外枠・マウス本体
        const hlslpp::float4 kCapFaceColor   = hlslpp::float4(0.94f, 0.94f, 0.97f, 1.00f);    //!< キーキャップの面
        const hlslpp::float4 kGlyphColor     = hlslpp::float4(0.06f, 0.06f, 0.09f, 1.00f);    //!< キーの文字（面が明るいので濃い色）
        const hlslpp::float4 kChevronColor   = hlslpp::float4(0.96f, 0.96f, 1.00f, 1.00f);    //!< 矢印
        const hlslpp::float4 kCaptionColor   = hlslpp::float4(1.00f, 1.00f, 1.00f, 1.00f);    //!< 下のテキスト
        const hlslpp::float4 kRingTrackColor = hlslpp::float4(1.00f, 1.00f, 1.00f, 0.20f);    //!< 長押しゲージの未点灯セグメント

        constexpr float kPi = 3.14159265f;

        //! WhitePixel.png。UI用テクスチャはこれとExpOrb.pngしか無く、単色矩形は全てこれをtintColorで着色して作る
        constexpr const char* kWhitePixelPath = "CombatAndroid/Assets/Textures/UI/WhitePixel.png";

        //-------------------------------------------------------------
        //! @brief  単色矩形スプライト用のエンティティを1つ作る
        //! @param  registry      [in] ECSレジストリ
        //! @param  textureHandle [in] WhitePixel.pngのハンドル
        //! @param  sortOrder     [in] 描画層
        //! @param  worldAnchored [in] WorldAnchorComponentを付けるか
        //! @return 作ったエンティティ
        //! @note   scale=0で作る（SpriteRenderSystemが面積ゼロのスプライトを描画から外すため、
        //!         表示されるまで1ドローコールも積まれない）
        //-------------------------------------------------------------
        [[nodiscard]]
        Tsukino::ECS::Entity CreateRectEntity(Tsukino::ECS::Registry& registry, const Tsukino::Asset::AssetHandle& textureHandle,
                                              int sortOrder, bool worldAnchored) {
            Tsukino::ECS::Entity entity = registry.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& transform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            transform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
            transform.dirty = true;

            Tsukino::BuiltIn::ECS::SpriteComponent& sprite = registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(entity);
            sprite.textureHandle                            = textureHandle;
            sprite.sortOrder                                = sortOrder;

            if(worldAnchored)
                registry.AddComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(entity);

            return entity;
        }

        //-------------------------------------------------------------
        //! @brief  文字用のエンティティを1つ作る
        //! @param  registry      [in] ECSレジストリ
        //! @param  sortOrder     [in] 描画層
        //! @param  worldAnchored [in] WorldAnchorComponentを付けるか
        //! @return 作ったエンティティ
        //! @note   fontHandle未設定 → builtinAssetsのdefaultFont（動的フォントアトラス）が
        //!         使われるので日本語をそのまま渡してよい。空文字の間は描画されない
        //-------------------------------------------------------------
        [[nodiscard]]
        Tsukino::ECS::Entity CreateTextEntity(Tsukino::ECS::Registry& registry, int sortOrder, bool worldAnchored) {
            Tsukino::ECS::Entity entity = registry.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& transform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            transform.dirty = true;

            Tsukino::BuiltIn::ECS::FontComponent& font = registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(entity);
            font.sortOrder                              = sortOrder;

            // キーキャップの面の中央に載せたいので、文字列の中心を描画位置に合わせる
            font.horizontalAlign = Tsukino::BuiltIn::ECS::HorizontalAlign::Center;
            font.verticalAlign   = Tsukino::BuiltIn::ECS::VerticalAlign::Middle;

            // 明るい面の上に濃い文字を置くため、縁取りは明るい色にして輪郭を立てる
            font.outlineColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 0.85f);
            font.outlineWidth = 1.0f;

            if(worldAnchored)
                registry.AddComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(entity);

            return entity;
        }

        //-------------------------------------------------------------
        //! @brief  キーキャップの幅を文字数から求める
        //! @param  label [in] キーに載せる文字
        //! @return キーキャップの面の幅（ピクセル）
        //-------------------------------------------------------------
        [[nodiscard]]
        float CalculateCapWidth(const std::wstring& label) {
            const float charCount = static_cast<float>(std::max<std::size_t>(label.size(), 1));
            return std::max(kCapMinWidth, kCapWidthPerChar * charCount + kCapBorder * 4.0f);
        }

        //-------------------------------------------------------------
        //! @struct PromptOrigin
        //! @brief  部品の相対位置をどこへ書くかの指定
        //! @note   画面固定モードはTransformComponent::positionへ直接書く。
        //!         ワールド追従モードはWorldAnchorComponent::screenOffsetへ書き、
        //!         positionはこの後に走るWorldAnchorSystemが投影結果で上書きする
        //-------------------------------------------------------------
        struct PromptOrigin {
            bool                 worldAnchored = false;
            float                screenX        = 0.0f;    //!< 画面固定モードでの中心X
            float                screenY        = 0.0f;    //!< 画面固定モードでの中心Y
            Tsukino::ECS::Entity target         = entt::null;
            hlslpp::float3       worldOffset{0.0f, 0.0f, 0.0f};
        };

        //-------------------------------------------------------------
        //! @brief  矩形の部品を1枚配置する
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  entity   [in] 対象のエンティティ
        //! @param  origin   [in] 位置の基準
        //! @param  offsetX  [in] 基準からの相対X（ピクセル）
        //! @param  offsetY  [in] 基準からの相対Y（ピクセル。下向きが正）
        //! @param  width    [in] 幅（ピクセル）
        //! @param  height   [in] 高さ（ピクセル）
        //! @param  roll     [in] 画面内の回転（ラジアン）
        //! @param  color    [in] 色
        //-------------------------------------------------------------
        void PlacePart(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, Tsukino::ECS::Entity entity,
                       const PromptOrigin& origin, float offsetX, float offsetY, float width, float height, float roll,
                       const hlslpp::float4& color) {
            if(entity == entt::null)
                return;

            if(origin.worldAnchored) {
                // 位置はWorldAnchorSystemが書くので、ここでは部品ごとのずらし量だけ渡す
                if(auto* anchor = registry.try_get<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(entity)) {
                    anchor->target       = origin.target;
                    anchor->worldOffset  = origin.worldOffset;
                    anchor->screenOffset = hlslpp::float2(offsetX, offsetY);
                }

                // 大きさ・回転・色だけを書く（positionは後段で上書きされるので0を渡してよい）
                StretchSpriteRotated(registry, context, entity, 0.0f, 0.0f, width, height, roll, color);
            } else {
                StretchSpriteRotated(registry, context, entity, origin.screenX + offsetX, origin.screenY + offsetY, width, height, roll,
                                     color);
            }
        }

        //-------------------------------------------------------------
        //! @brief  文字の部品を1つ配置する
        //! @param  registry  [in] ECSレジストリ
        //! @param  entity    [in] 対象のエンティティ
        //! @param  origin    [in] 位置の基準
        //! @param  offsetX   [in] 基準からの相対X（ピクセル）
        //! @param  offsetY   [in] 基準からの相対Y（ピクセル）
        //! @param  fontScale [in] 文字の拡大率
        //! @param  text      [in] 表示する文字列
        //! @param  color     [in] 文字色
        //-------------------------------------------------------------
        void PlaceTextPart(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, const PromptOrigin& origin, float offsetX,
                           float offsetY, float fontScale, const std::wstring& text, const hlslpp::float4& color) {
            if(entity == entt::null)
                return;

            if(origin.worldAnchored) {
                if(auto* anchor = registry.try_get<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(entity)) {
                    anchor->target       = origin.target;
                    anchor->worldOffset  = origin.worldOffset;
                    anchor->screenOffset = hlslpp::float2(offsetX, offsetY);
                }

                PlaceUiText(registry, entity, 0.0f, 0.0f, fontScale, text, color);
            } else {
                PlaceUiText(registry, entity, origin.screenX + offsetX, origin.screenY + offsetY, fontScale, text, color);
            }
        }

        //-------------------------------------------------------------
        //! @brief  色に不透明度を掛ける
        //! @param  color [in] 元の色
        //! @param  alpha [in] 掛ける不透明度
        //! @return アルファを掛けた色
        //-------------------------------------------------------------
        [[nodiscard]]
        hlslpp::float4 ApplyAlpha(const hlslpp::float4& color, float alpha) {
            return color * hlslpp::float4(1.0f, 1.0f, 1.0f, alpha);
        }

        //-------------------------------------------------------------
        //! @brief  プロンプト一式を1フレームぶん配置する（画面固定・ワールド追従で共通）
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  widget   [in] 対象のウィジェット
        //! @param  origin   [in] 位置の基準
        //! @param  style    [in] 見た目の指定
        //-------------------------------------------------------------
        void LayoutPrompt(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                          const InputPromptWidget& widget, const PromptOrigin& origin, const InputPromptStyle& style) {
            const float s     = style.scale;
            const float alpha = std::clamp(style.alpha, 0.0f, 1.0f);

            //-------------------------------------------------------------
            // キーキャップ／マウス本体。どちらも「濃い矩形の上に明るい矩形を重ねる」構造で、
            // 明るい矩形がキーの面／押している左ボタンにあたる
            //-------------------------------------------------------------
            float bodyWidth  = 0.0f;
            float bodyHeight = 0.0f;

            if(widget.useMouse) {
                bodyWidth  = kMouseWidth * s;
                bodyHeight = kMouseHeight * s;

                PlacePart(registry, context, widget.capBorderEntity, origin, 0.0f, 0.0f, bodyWidth, bodyHeight, 0.0f,
                          ApplyAlpha(kCapBorderColor, alpha));

                // 左ボタンは本体の左上に寄せる。ピボットが中心なので、
                // 「本体の左上角 ＋ 内側への差し込み ＋ ボタン自身の半分」で中心を出す
                const float buttonWidth  = kMouseButtonWidth * s;
                const float buttonHeight = kMouseButtonHeight * s;
                const float buttonX      = -bodyWidth * 0.5f + kMouseButtonInset * s + buttonWidth * 0.5f;
                const float buttonY      = -bodyHeight * 0.5f + kMouseButtonInset * s + buttonHeight * 0.5f;

                // 押している左ボタンだけを溜め段階の色で光らせ、リングの色と揃える
                PlacePart(registry, context, widget.capFaceEntity, origin, buttonX, buttonY, buttonWidth, buttonHeight, 0.0f,
                          ApplyAlpha(style.accentColor, alpha));

                HideUiText(registry, widget.glyphEntity);
            } else if(widget.capBorderEntity != entt::null) {
                // キーの文字は生成時に入れたきり変えないので、幅の計算にもそれをそのまま使う
                const auto* glyphFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(widget.glyphEntity);
                const float faceWidth = (glyphFont ? CalculateCapWidth(glyphFont->text) : kCapMinWidth) * s;

                bodyWidth  = faceWidth + kCapBorder * 2.0f * s;
                bodyHeight = (kCapHeight + kCapBorder * 2.0f) * s;

                PlacePart(registry, context, widget.capBorderEntity, origin, 0.0f, 0.0f, bodyWidth, bodyHeight, 0.0f,
                          ApplyAlpha(kCapBorderColor, alpha));
                PlacePart(registry, context, widget.capFaceEntity, origin, 0.0f, 0.0f, faceWidth, kCapHeight * s, 0.0f,
                          ApplyAlpha(kCapFaceColor, alpha));
            }

            //-------------------------------------------------------------
            // キーの文字。テキストの中身は呼び出し側が生成時に入れたものをそのまま使う
            // （毎フレーム書き換えないので、位置と色だけ更新する）
            //-------------------------------------------------------------
            if(!widget.useMouse && widget.glyphEntity != entt::null) {
                if(auto* glyphFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(widget.glyphEntity))
                    PlaceTextPart(registry, widget.glyphEntity, origin, 0.0f, 0.0f, kGlyphScale * s, glyphFont->text,
                                  ApplyAlpha(kGlyphColor, alpha));
            }

            //-------------------------------------------------------------
            // 矢印。細い矩形2枚を±45度に倒してハの字に組む。
            // 画面座標は下向きがY正なので、上向きの∧は2本の腕を上側で合わせる形になる
            //-------------------------------------------------------------
            if(widget.chevron != PromptChevron::None) {
                const float armLength = kChevronArmLength * s;
                const float armWidth  = kChevronArmWidth * s;
                const float spread    = kChevronArmSpread * s;
                const float gap       = kChevronGap * s;
                const float quarter   = kPi * 0.25f;

                float centerX = 0.0f;
                float centerY = 0.0f;
                float rollA   = 0.0f;
                float rollB   = 0.0f;
                float armAX   = 0.0f;
                float armAY   = 0.0f;
                float armBX   = 0.0f;
                float armBY   = 0.0f;

                switch(widget.chevron) {
                    case PromptChevron::Up:
                        centerY = -(bodyHeight * 0.5f + gap);
                        rollA    = quarter;
                        rollB    = -quarter;
                        armAX    = -spread;
                        armBX    = spread;
                        break;

                    case PromptChevron::Down:
                        centerY = bodyHeight * 0.5f + gap;
                        rollA    = -quarter;
                        rollB    = quarter;
                        armAX    = -spread;
                        armBX    = spread;
                        break;

                    case PromptChevron::Right:
                        centerX = bodyWidth * 0.5f + gap;
                        rollA    = -quarter;
                        rollB    = quarter;
                        armAY    = -spread;
                        armBY    = spread;
                        break;

                    default:
                        break;
                }

                PlacePart(registry, context, widget.chevronArmAEntity, origin, centerX + armAX, centerY + armAY, armWidth, armLength, rollA,
                          ApplyAlpha(kChevronColor, alpha));
                PlacePart(registry, context, widget.chevronArmBEntity, origin, centerX + armBX, centerY + armBY, armWidth, armLength, rollB,
                          ApplyAlpha(kChevronColor, alpha));
            }

            //-------------------------------------------------------------
            // 長押しゲージ。真上から時計回りに、進行度ぶんだけ点灯色、残りはトラック色で出す。
            // 「あとどれだけ押すのか」が見えるよう、未点灯ぶんも薄く残すのが肝
            //-------------------------------------------------------------
            if(widget.ringEntities[0] != entt::null) {
                const float progress = std::clamp(style.holdProgress, 0.0f, 1.0f);
                const float litCount = progress * static_cast<float>(kHoldRingSegmentCount);

                for(int i = 0; i < kHoldRingSegmentCount; ++i) {
                    // 真上（-Y方向）を起点に時計回り。画面座標は下向きがY正なので、
                    // Yはcosの符号を反転させる
                    const float angle   = static_cast<float>(i) * (2.0f * kPi / static_cast<float>(kHoldRingSegmentCount));
                    const float radius  = kRingRadius * s;
                    const float offsetX = std::sin(angle) * radius;
                    const float offsetY = -std::cos(angle) * radius;

                    const bool            lit   = static_cast<float>(i) < litCount;
                    const hlslpp::float4& color = lit ? style.accentColor : kRingTrackColor;

                    // セグメントは半径方向を向く目盛りにしたいので、真上の1枚を基準に同じ角度だけ回す
                    PlacePart(registry, context, widget.ringEntities[static_cast<std::size_t>(i)], origin, offsetX, offsetY,
                              kRingSegmentWidth * s, kRingSegmentLength * s, angle, ApplyAlpha(color, alpha));
                }
            }

            //-------------------------------------------------------------
            // 下に添えるテキスト（対象の名前など）
            //-------------------------------------------------------------
            if(widget.captionEntity != entt::null)
                PlaceTextPart(registry, widget.captionEntity, origin, 0.0f, bodyHeight * 0.5f + kCaptionGap * s, kCaptionScale * s,
                              style.caption, ApplyAlpha(kCaptionColor, alpha));
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief プロンプトのエンティティ一式を生成する
    //-------------------------------------------------------------
    InputPromptWidget CreateInputPromptWidget(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                              const InputPromptDesc& desc) {
        InputPromptWidget widget;
        widget.useMouse = desc.useMouse;
        widget.chevron  = desc.chevron;

        if(!context.assetManager)
            return widget;

        // AssetManagerはパスでキャッシュするので、何度Loadを呼んでも実読み込みは1回
        Tsukino::Asset::AssetHandle whitePixelHandle = context.assetManager->Load(Tsukino::Core::Path(kWhitePixelPath));

        //-------------------------------------------------------------
        // 描画層はsortOrderBaseから+4までの5層を使う。
        // 奥から: リングのトラック → リングの点灯 → キャップ外枠 → キャップ面・矢印 → 文字
        //-------------------------------------------------------------
        const int ringSortOrder    = desc.sortOrderBase;
        const int borderSortOrder  = desc.sortOrderBase + 2;
        const int faceSortOrder    = desc.sortOrderBase + 3;
        const int glyphSortOrder   = desc.sortOrderBase + 4;

        if(desc.useHoldRing) {
            for(auto& ringEntity : widget.ringEntities)
                ringEntity = CreateRectEntity(registry, whitePixelHandle, ringSortOrder, desc.worldAnchored);
        }

        if(desc.useMouse || !desc.keyLabel.empty()) {
            widget.capBorderEntity = CreateRectEntity(registry, whitePixelHandle, borderSortOrder, desc.worldAnchored);
            widget.capFaceEntity   = CreateRectEntity(registry, whitePixelHandle, faceSortOrder, desc.worldAnchored);
        }

        if(!desc.useMouse && !desc.keyLabel.empty()) {
            widget.glyphEntity = CreateTextEntity(registry, glyphSortOrder, desc.worldAnchored);

            // キーの文字は変わらないのでここで入れ、以後は位置と色だけ更新する
            if(auto* glyphFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(widget.glyphEntity))
                glyphFont->text = desc.keyLabel;
        }

        if(desc.chevron != PromptChevron::None) {
            widget.chevronArmAEntity = CreateRectEntity(registry, whitePixelHandle, faceSortOrder, desc.worldAnchored);
            widget.chevronArmBEntity = CreateRectEntity(registry, whitePixelHandle, faceSortOrder, desc.worldAnchored);
        }

        if(desc.useCaption) {
            widget.captionEntity = CreateTextEntity(registry, glyphSortOrder, desc.worldAnchored);

            // 対象名は背景がワールドなので、濃い縁取りで抜く（キーの文字とは逆）
            if(auto* captionFont = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(widget.captionEntity)) {
                captionFont->outlineColor = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.9f);
                captionFont->outlineWidth = 2.0f;
            }
        }

        return widget;
    }

    //-------------------------------------------------------------
    //! @brief 画面固定のプロンプトを配置して表示する
    //-------------------------------------------------------------
    void ShowInputPromptAtScreen(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                 const InputPromptWidget& widget, float centerX, float centerY, const InputPromptStyle& style) {
        PromptOrigin origin;
        origin.worldAnchored = false;
        origin.screenX        = centerX;
        origin.screenY        = centerY;

        LayoutPrompt(registry, context, widget, origin, style);
    }

    //-------------------------------------------------------------
    //! @brief ワールド上の対象へ追従するプロンプトを表示する
    //-------------------------------------------------------------
    void ShowInputPromptAtWorld(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                const InputPromptWidget& widget, Tsukino::ECS::Entity target, const hlslpp::float3& worldOffset,
                                const InputPromptStyle& style) {
        if(target == entt::null || !registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(target)) {
            HideInputPrompt(registry, widget);
            return;
        }

        //-------------------------------------------------------------
        // 前フレームに画面外・カメラ後方だったなら丸ごと隠す。
        // WorldAnchorSystemはvisible=falseのときpositionを更新せず、
        // SpriteRendererSystemはvisibleを見ないため、隠さないと最後に映っていた
        // 画面端の位置に部品が貼り付いたまま残ってしまう
        //-------------------------------------------------------------
        if(widget.capBorderEntity != entt::null) {
            if(const auto* anchor = registry.try_get<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(widget.capBorderEntity)) {
                if(anchor->target == target && !anchor->visible) {
                    HideInputPrompt(registry, widget);
                    return;
                }
            }
        }

        PromptOrigin origin;
        origin.worldAnchored = true;
        origin.target        = target;
        origin.worldOffset   = worldOffset;

        LayoutPrompt(registry, context, widget, origin, style);
    }

    //-------------------------------------------------------------
    //! @brief プロンプトを丸ごと非表示にする
    //-------------------------------------------------------------
    void HideInputPrompt(Tsukino::ECS::Registry& registry, const InputPromptWidget& widget) {
        HideUiSprite(registry, widget.capBorderEntity);
        HideUiSprite(registry, widget.capFaceEntity);
        HideUiSprite(registry, widget.chevronArmAEntity);
        HideUiSprite(registry, widget.chevronArmBEntity);

        for(const auto& ringEntity : widget.ringEntities)
            HideUiSprite(registry, ringEntity);

        //-------------------------------------------------------------
        // 文字は空文字にすると描画されないが、キーの文字（glyph）だけは
        // レイアウト時に幅の計算へ使うので消さず、WorldAnchorのtargetを外して隠す
        //-------------------------------------------------------------
        if(widget.glyphEntity != entt::null) {
            if(auto* anchor = registry.try_get<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(widget.glyphEntity))
                anchor->target = entt::null;

            if(auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(widget.glyphEntity)) {
                transform->scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
                transform->dirty = true;
            }
        }

        HideUiText(registry, widget.captionEntity);
    }
}    // namespace CombatAndroid::ECS
