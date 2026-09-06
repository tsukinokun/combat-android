//-------------------------------------------------------------
//! @file    UiSprite.hpp
//! @brief   画面固定UIのスプライト・文字を配置する共通処理の宣言
//! @author  山﨑愛
//! @note    SpriteComponentは「位置・大きさ」を持たず、TransformComponentの
//!          position（画面ピクセル座標）とscale（テクスチャ実寸への倍率）で決まる。
//!          その変換を各Systemが個別に書くと、WhitePixel.png（4x4）決め打ちの
//!          割り算があちこちに散らばり、専用画像へ差し替えた途端に全部崩れる。
//!          ここへ集約し、テクスチャの実寸はAssetManagerから引く
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <hlsl++.h>

#include <string>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  スプライトを中心座標・ピクセル寸法・色で配置する
    //! @param  registry  [in] ECSレジストリ
    //! @param  context   [in] エンジンコンテキスト（テクスチャの実寸を引くのに使う）
    //! @param  entity    [in] 対象のエンティティ（TransformComponent + SpriteComponentが必要）
    //! @param  centerX   [in] 矩形中心のスクリーンX
    //! @param  centerY   [in] 矩形中心のスクリーンY
    //! @param  width     [in] 矩形の幅（ピクセル）
    //! @param  height    [in] 矩形の高さ（ピクセル）
    //! @param  tintColor [in] スプライトに乗算する色
    //! @note   スプライトのピボットは常に中心（Quadの頂点が-0.5〜+0.5）。
    //!         左端を固定したい場合は呼び出し側でcenterXをずらす
    //-------------------------------------------------------------
    void StretchSprite(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, Tsukino::ECS::Entity entity,
                       float centerX, float centerY, float width, float height, const hlslpp::float4& tintColor);

    //-------------------------------------------------------------
    //! @brief  スプライトを中心座標・ピクセル寸法・色・回転で配置する
    //! @param  registry  [in] ECSレジストリ
    //! @param  context   [in] エンジンコンテキスト（テクスチャの実寸を引くのに使う）
    //! @param  entity    [in] 対象のエンティティ
    //! @param  centerX   [in] 矩形中心のスクリーンX
    //! @param  centerY   [in] 矩形中心のスクリーンY
    //! @param  width     [in] 矩形の幅（ピクセル）
    //! @param  height    [in] 矩形の高さ（ピクセル）
    //! @param  rollRadians [in] 画面内での回転角（ラジアン。時計回りが正）
    //! @param  tintColor [in] スプライトに乗算する色
    //! @note   Screen空間のスプライトはworldMatrix経由で描かれるため、TransformComponent::rotationが
    //!         そのまま効く（SpriteRendererSystemが「親子関係と回転が効く」と明記している）。
    //!         矢印やリングゲージのように矩形を傾けて組む用途で使う
    //-------------------------------------------------------------
    void StretchSpriteRotated(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                              Tsukino::ECS::Entity entity, float centerX, float centerY, float width, float height, float rollRadians,
                              const hlslpp::float4& tintColor);

    //-------------------------------------------------------------
    //! @brief  文字エンティティの位置・大きさ・内容を書く
    //! @param  registry  [in] ECSレジストリ
    //! @param  entity    [in] 対象のエンティティ（TransformComponent + FontComponentが必要）
    //! @param  x         [in] スクリーンX
    //! @param  y         [in] スクリーンY
    //! @param  fontScale [in] フォントの拡大率（TransformComponent::scale.xがそのまま文字サイズになる）
    //! @param  text      [in] 表示する文字列
    //! @param  color     [in] 文字色
    //-------------------------------------------------------------
    void PlaceUiText(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float x, float y, float fontScale,
                     const std::wstring& text, const hlslpp::float4& color);

    //-------------------------------------------------------------
    //! @brief  スプライトを非表示にする
    //! @param  registry [in] ECSレジストリ
    //! @param  entity   [in] 対象のエンティティ
    //! @note   scale=0にする。SpriteRendererSystemはWorldAnchorComponent::visibleを見ておらず
    //!         （見ているのはFontRendererSystemだけ）、面積ゼロのスプライトだけを描画から
    //!         外している。tintColor.wを0にするやり方だと透明な板がドローコールを積み続ける
    //-------------------------------------------------------------
    void HideUiSprite(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);

    //-------------------------------------------------------------
    //! @brief  文字エンティティを非表示にする
    //! @param  registry [in] ECSレジストリ
    //! @param  entity   [in] 対象のエンティティ
    //! @note   空文字にする（FontRendererSystemは空文字を描画しない）
    //-------------------------------------------------------------
    void HideUiText(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);
}    // namespace CombatAndroid::ECS
