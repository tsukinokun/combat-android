//-------------------------------------------------------------
//! @file    UiMotion.hpp
//! @brief   UIの動きの定型（出現・消える・弾む・呼吸・まとめて動かす）の宣言
//! @note    中身はエンジンのUIトゥイーン（Tsukino::BuiltIn::ECS::UITweenUtility::Play）を包んだもの。
//!          動きは見た目のずれ（UIMotionComponent）と透明度（UICanvasGroupComponent）にだけ掛かり、
//!          各システムが毎フレーム書くTransform・色とはぶつからない。
//!          秒数・距離・強さは Assets/Tables/Systems/UiMotion.json が持つ（再ビルド不要）。
//!          メニュー中にも動かすもの（unscaledTime）は、シーンが渡すUnscaledTimeContextの実時間で進む
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <span>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct InputPromptWidget;

    //-------------------------------------------------------------
    //! @struct UiMotionParams
    //! @brief  UIの動きの既定値（Assets/Tables/Systems/UiMotion.json）
    //-------------------------------------------------------------
    struct UiMotionParams {
        float appearDuration    = 0.28f;     //!< 出現にかける秒数
        float appearSlide       = 36.0f;     //!< 出現で滑り込む距離（ピクセル）
        float appearStagger     = 0.05f;     //!< 並んだ項目を順に出すときの、1項目ごとの遅れ（秒）
        float popScaleFrom      = 0.6f;      //!< ポップの始めの拡大率（OutBackで1へ少し行き過ぎて戻る）
        float popDuration       = 0.35f;     //!< ポップにかける秒数
        float disappearDuration = 0.18f;     //!< 消えるのにかける秒数
        float disappearSlide    = 16.0f;     //!< 消えるときに流れる距離（ピクセル）
        float punchStrength     = 0.12f;     //!< 弾みの強さ（拡大率の振れ幅）
        float punchDuration     = 0.3f;      //!< 弾みの秒数
        float pulseAmount       = 0.06f;     //!< 呼吸の拡大率の振れ幅
        float pulsePeriod       = 0.9f;      //!< 呼吸の片道の秒数
        float slideDuration     = 0.14f;     //!< 選択帯などが前の位置から今の位置へ移る秒数
        float idleBob           = 6.0f;      //!< ゆらゆらの上下の振れ幅（ピクセル）
        float idlePeriod        = 1.4f;      //!< ゆらゆらの上下の片道の秒数
        float idleTilt          = 0.8f;      //!< ゆらゆらの傾きの振れ幅（度。左右に振る）
        float idleTiltPeriod    = 2.3f;      //!< ゆらゆらの傾きの片道の秒数（上下と周期をずらして機械的に見えないようにする）
    };

    //-------------------------------------------------------------
    //! @brief  UIの動きの既定値を得る関数（初回に1度だけJSONを読む）
    //-------------------------------------------------------------
    [[nodiscard]]
    const UiMotionParams& GetUiMotionParams();

    //-------------------------------------------------------------
    //! @struct UiAppearDesc
    //! @brief  出現のしかた
    //-------------------------------------------------------------
    struct UiAppearDesc {
        float slideX       = 0.0f;     //!< 始めのずれX（ピクセル。ここから0へ滑り込む）
        float slideY       = 0.0f;     //!< 始めのずれY
        float scaleFrom    = 1.0f;     //!< 始めの拡大率（1なら拡大しない。1未満ならOutBackでポップ）
        float delay        = 0.0f;     //!< 始めるまでの待ち（秒）
        float duration     = -1.0f;    //!< 秒数（負なら既定値。ポップならpopDuration、それ以外はappearDuration）
        bool  unscaledTime = false;    //!< メニュー中・スロー中も実時間で進めるか
    };

    //-------------------------------------------------------------
    //! @brief  出現させる（透明→不透明・滑り込む・ポップ）
    //! @param  registry [in,out] ECSレジストリ
    //! @param  entity   [in]     動かすエンティティ（UiFollowで追従させた部品も一緒に動く）
    //! @param  desc     [in]     出現のしかた
    //! @note   直前の出現・消えるは置き換える（途中から呼んでも重ならない）
    //-------------------------------------------------------------
    void UiAppear(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, const UiAppearDesc& desc);

    //-------------------------------------------------------------
    //! @brief  下から滑り込んで出す（よく使う形）
    //-------------------------------------------------------------
    void UiAppearFromBelow(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float delay = 0.0f, bool unscaledTime = false);

    //-------------------------------------------------------------
    //! @brief  横から滑り込んで出す（メニューの項目を上から順に出す）
    //! @param  index [in] 並びの何番目か（appearStagger×indexだけ遅らせる）
    //-------------------------------------------------------------
    void UiAppearFromSide(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, int index, bool unscaledTime);

    //-------------------------------------------------------------
    //! @brief  ポップで出す（OutBackで少し大きくなってから落ち着く）
    //-------------------------------------------------------------
    void UiPopIn(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float delay = 0.0f, bool unscaledTime = false);

    //-------------------------------------------------------------
    //! @brief  消す（不透明→透明・少し流れる）。終わっても透明のまま保つ
    //! @note   実際に隠す（scale=0・文字を空にする）のは呼び出し側。UiIsVisibilityAnimatingがfalseになってから隠す
    //-------------------------------------------------------------
    void UiDisappear(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, bool unscaledTime = false);

    //-------------------------------------------------------------
    //! @brief  出現・消えるの動きが残っているか（消えたあと透明のまま保っている間はfalse）
    //-------------------------------------------------------------
    [[nodiscard]]
    bool UiIsVisibilityAnimating(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);

    //-------------------------------------------------------------
    //! @brief  出現・消えるの動きを外して、素の見た目へ戻す
    //-------------------------------------------------------------
    void UiResetVisibility(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);

    //-------------------------------------------------------------
    //! @brief  前の位置からなめらかに移ってきたように見せる（選択帯がカーソルを追う等）
    //! @param  fromX [in] 前の位置から今の位置を引いたずれX（ここから0へ寄せる）
    //! @param  fromY [in] 同じくY
    //! @note   位置そのものは呼び出し側が先に今の位置へ書いておく。途中で呼び直すと、今見えている位置から続けて寄せる
    //-------------------------------------------------------------
    void UiSlideFrom(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float fromX, float fromY, bool unscaledTime = false);

    //-------------------------------------------------------------
    //! @brief  弾ませる（決定・被弾・値の更新の手ごたえ）
    //! @param  strength [in] 強さの倍率（1で既定のpunchStrength）
    //-------------------------------------------------------------
    void UiPunch(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float strength = 1.0f, bool unscaledTime = false);

    //-------------------------------------------------------------
    //! @brief  呼吸させる（ゆっくり拡大縮小を繰り返す）。UiStopPulseまで続く
    //-------------------------------------------------------------
    void UiPulse(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float amountScale = 1.0f, bool unscaledTime = false);

    //-------------------------------------------------------------
    //! @brief  ゆらゆらさせる（ゆっくり浮き沈みしながら、わずかに傾く）。UiStopIdleまで続く
    //! @param  amountScale  [in] 振れ幅に掛ける倍率（小さい文字は控えめに）
    //! @param  unscaledTime [in] メニュー中・スロー中も実時間で進めるか
    //! @note   見出しを止まったままにしないための待機中の動き。出現・弾む・呼吸と重ねて使える。
    //!         すでにゆらゆらしていれば何もしないので、毎フレーム呼んでもよい
    //-------------------------------------------------------------
    void UiIdle(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float amountScale = 1.0f, bool unscaledTime = false);

    //-------------------------------------------------------------
    //! @brief  ゆらゆらを止める
    //-------------------------------------------------------------
    void UiStopIdle(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);

    //-------------------------------------------------------------
    //! @brief  呼吸を止める
    //-------------------------------------------------------------
    void UiStopPulse(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);

    //-------------------------------------------------------------
    //! @brief  呼吸しているか
    //-------------------------------------------------------------
    [[nodiscard]]
    bool UiIsPulsing(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);

    //-------------------------------------------------------------
    //! @brief  部品を親役のエンティティの動きに追従させる（板の上の文字・キー表示を板と一緒に動かす）
    //! @param  follower [in] 追従させる部品（entt::nullなら何もしない）
    //! @param  source   [in] 動きの元（UiAppear等で動かすエンティティ）
    //! @note   どちらも画面ピクセル座標の画面UIであること。一度呼べば以後ずっと追従する
    //-------------------------------------------------------------
    void UiFollow(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity follower, Tsukino::ECS::Entity source);

    //-------------------------------------------------------------
    //! @brief  複数の部品をまとめて追従させる
    //-------------------------------------------------------------
    void UiFollowAll(Tsukino::ECS::Registry& registry, std::span<const Tsukino::ECS::Entity> followers, Tsukino::ECS::Entity source);

    //-------------------------------------------------------------
    //! @brief  キー表示（InputPromptWidget）の部品をまとめて追従させる
    //-------------------------------------------------------------
    void UiFollowPrompt(Tsukino::ECS::Registry& registry, const InputPromptWidget& prompt, Tsukino::ECS::Entity source);
}    // namespace CombatAndroid::ECS
