//-------------------------------------------------------------
//! @file    UiTextSize.hpp
//! @brief   画面の文字の大きさの段階の宣言
//! @note    文字の大きさは画面ごとに数値で決めず、必ずこの段階のどれかを使う。
//!          段階ごとの倍率は Assets/Tables/UiTextSize.json が持つので、
//!          1つ書き換えればその段階の文字が全画面でそろって変わる（再ビルド不要）
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum   UiTextSize
    //! @brief  文字の大きさの段階（大きい順）
    //-------------------------------------------------------------
    enum class UiTextSize {
        Display,    //!< タイトル画面のゲーム名
        Title,      //!< 画面の見出し（PAUSE・リザルトの見出し・LEVEL UP!）
        Heading,    //!< 小見出し（オプション・操作説明・NOW LOADING）
        Large,      //!< 目立たせる値と選択肢（メニューの選択肢・スキル名・リザルトの値・生存時間・危険度）
        Body,       //!< 本文（説明文・取得ログ・チュートリアル・操作説明の行・取得済みスキル）
        Small,      //!< 注記（HP/EXPの数値・ログの種別・記録・ベスト・案内・カウンター）
    };

    //-------------------------------------------------------------
    //! @brief  段階の倍率を得る関数
    //! @param  size [in] 段階
    //! @return 文字の拡大率（TransformComponent::scale にそのまま入れる。基準は .dfont の Size）
    //! @note   初回の呼び出しで Assets/Tables/UiTextSize.json を1度だけ読む
    //-------------------------------------------------------------
    [[nodiscard]]
    float GetUiTextScale(UiTextSize size);
}    // namespace CombatAndroid::ECS
