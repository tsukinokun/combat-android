//-------------------------------------------------------------
//! @file    InputPromptWidget.hpp
//! @brief   操作を促すUI（キーキャップ・マウス・矢印・長押しゲージ）の組み立てと更新の宣言
//! @author  山﨑愛
//! @note    「文字で操作を説明せず、図で分からせる」ためのUI部品。
//!          SpriteComponentにはラジアルフィルも9スライスもピボット指定も無く、
//!          uvRectに至ってはレンダラーが読んでいない。使えるのは
//!          「WhitePixel.pngをtintColorで着色した矩形」と「TransformComponent::rotationによる回転」
//!          （Screen空間でも効く）の2つだけなので、絵は全て矩形の組み合わせで作る。
//!          長押しゲージも扇形ではなく、円周に並べた矩形セグメントを順に点灯させて表す
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <hlsl++.h>

#include <array>
#include <string>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! 長押しゲージのセグメント数。扇形フィルが使えないので、この数の矩形を
    //! 円周に等間隔で並べ、進行度に応じて手前から点灯させる。
    //! 増やすほど滑らかになるがエンティティ数もそのまま増える（16で1周22.5度）
    //-------------------------------------------------------------
    inline constexpr int kHoldRingSegmentCount = 16;

    //-------------------------------------------------------------
    //! @enum   PromptChevron
    //! @brief  キーキャップに添える矢印の向き
    //! @note   細い矩形2枚をハの字に組んで作る。「何をする操作なのか」を
    //!         文字を使わずに示すための唯一の語彙
    //-------------------------------------------------------------
    enum class PromptChevron {
        None = 0,    //!< 矢印なし
        Up,          //!< ∧（拾い上げる・上のカードへ）
        Down,        //!< ∨（下のカードへ）
        Right,       //!< ＞（決定・もう一度）
    };

    //-------------------------------------------------------------
    //! @struct InputPromptDesc
    //! @brief  プロンプト1個をどう組み立てるかの指定
    //-------------------------------------------------------------
    struct InputPromptDesc {
        std::wstring keyLabel;    //!< キーキャップに載せる文字（"F" / "SPACE" 等）。空かつuseMouse=falseならキーキャップを作らない

        bool useMouse   = false;    //!< キーキャップの代わりにマウスの絵を出す（左クリック操作用）
        bool useHoldRing = false;    //!< 長押しゲージ（円周のセグメント）を作る
        bool useCaption  = false;    //!< 下に添える1行テキストを作る（対象の名前など、操作の説明ではない情報用）

        PromptChevron chevron = PromptChevron::None;    //!< 添える矢印の向き

        //! ワールド上の対象へ追従させるか。trueなら全部品にWorldAnchorComponentを付け、
        //! 部品ごとの相対位置はscreenOffsetで作る（HealthBarSystemのHPバーと同じ手口）。
        //! falseなら画面固定で、位置は毎フレーム直接書く
        bool worldAnchored = false;

        //! 描画層のベース値。ここから+4までの5層を使うのでUiSortOrder.hppの
        //! kInputPromptBase / kModalInputPromptBase を渡すこと
        int sortOrderBase = 0;
    };

    //-------------------------------------------------------------
    //! @struct InputPromptWidget
    //! @brief  プロンプト1個ぶんのエンティティ束
    //! @note   生成は初期化時に1回だけ行い、以後は表示/非表示と値の更新で使い回す
    //!         （「Fキーで拾う」ラベルやダメージ数値と同じく、毎フレーム生成しない）。
    //!         使わない部品はentt::nullのまま残り、更新関数側が黙って読み飛ばす
    //-------------------------------------------------------------
    struct InputPromptWidget {
        Tsukino::ECS::Entity capBorderEntity   = entt::null;    //!< キーキャップの外枠 / マウス本体（濃い矩形）
        Tsukino::ECS::Entity capFaceEntity     = entt::null;    //!< キーキャップの面 / マウスの左ボタン（明るい矩形）
        Tsukino::ECS::Entity glyphEntity       = entt::null;    //!< 面に載るキーの文字
        Tsukino::ECS::Entity chevronArmAEntity = entt::null;    //!< 矢印の片腕
        Tsukino::ECS::Entity chevronArmBEntity = entt::null;    //!< 矢印のもう片腕
        Tsukino::ECS::Entity captionEntity     = entt::null;    //!< 下に添える1行テキスト

        std::array<Tsukino::ECS::Entity, kHoldRingSegmentCount> ringEntities{};    //!< 長押しゲージのセグメント（先頭が真上、時計回り）

        bool          useMouse = false;                     //!< マウスの絵として組んだか（更新時のレイアウト分岐に使う）
        PromptChevron chevron  = PromptChevron::None;       //!< どの向きの矢印を組んだか
    };

    //-------------------------------------------------------------
    //! @brief  プロンプトのエンティティ一式を生成する
    //! @param  registry [in] ECSレジストリ
    //! @param  context  [in] エンジンコンテキスト（WhitePixel.pngのロードに使う）
    //! @param  desc     [in] 何を作るかの指定
    //! @return 生成したエンティティ束
    //! @note   生成直後は全て非表示（scale=0／空文字）。表示はShow系の関数が行う
    //-------------------------------------------------------------
    [[nodiscard]]
    InputPromptWidget CreateInputPromptWidget(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                              const InputPromptDesc& desc);

    //-------------------------------------------------------------
    //! @struct InputPromptStyle
    //! @brief  1フレームぶんの見た目の指定
    //-------------------------------------------------------------
    struct InputPromptStyle {
        float scale = 1.0f;    //!< 全体の拡大率（1.0で既定サイズ）
        float alpha = 1.0f;    //!< 全体の不透明度

        //! 長押しゲージの進行度（0〜1）。useHoldRingのときだけ意味を持つ
        float holdProgress = 0.0f;

        //! 長押しゲージの点灯色・マウスの左ボタンの色。溜め段階の色をそのまま渡す
        hlslpp::float4 accentColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);

        std::wstring caption;    //!< useCaptionのときに出す1行テキスト
    };

    //-------------------------------------------------------------
    //! @brief  画面固定のプロンプトを配置して表示する
    //! @param  registry [in] ECSレジストリ
    //! @param  context  [in] エンジンコンテキスト
    //! @param  widget   [in] 対象のウィジェット
    //! @param  centerX  [in] 中心のスクリーンX
    //! @param  centerY  [in] 中心のスクリーンY
    //! @param  style    [in] 見た目の指定
    //-------------------------------------------------------------
    void ShowInputPromptAtScreen(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                 const InputPromptWidget& widget, float centerX, float centerY, const InputPromptStyle& style);

    //-------------------------------------------------------------
    //! @brief  ワールド上の対象へ追従するプロンプトを表示する
    //! @param  registry    [in] ECSレジストリ
    //! @param  context     [in] エンジンコンテキスト
    //! @param  widget      [in] 対象のウィジェット
    //! @param  target      [in] 追従先エンティティ（TransformComponent必須）
    //! @param  worldOffset [in] 追従先からのワールドオフセット（頭上に出すなど）
    //! @param  style       [in] 見た目の指定
    //! @note   実際のスクリーン座標はWorldAnchorSystemが後から書くので、ここでは
    //!         大きさ・色・部品ごとの相対位置（screenOffset）だけを決める。
    //!         前フレームに画面外だった（visible==false）ときは丸ごと隠す ――
    //!         SpriteRendererSystemはvisibleを見ず、位置も更新されないままなので、
    //!         隠さないと画面端に部品が貼り付いて残る
    //-------------------------------------------------------------
    void ShowInputPromptAtWorld(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                const InputPromptWidget& widget, Tsukino::ECS::Entity target, const hlslpp::float3& worldOffset,
                                const InputPromptStyle& style);

    //-------------------------------------------------------------
    //! @brief  プロンプトを丸ごと非表示にする
    //! @param  registry [in] ECSレジストリ
    //! @param  widget   [in] 対象のウィジェット
    //-------------------------------------------------------------
    void HideInputPrompt(Tsukino::ECS::Registry& registry, const InputPromptWidget& widget);
}    // namespace CombatAndroid::ECS
