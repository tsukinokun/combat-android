//-------------------------------------------------------------
//! @file   UiSortOrder.hpp
//! @brief  UI（RenderPass::Overlay）の重なり順を一箇所に集めた定数
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::UI
namespace CombatAndroid::UI {
    //-------------------------------------------------------------
    // 値が小さいほど奥（先に描かれる）。
    // SpriteComponent::sortOrder と FontComponent::sortOrder は Renderer の
    // Overlayパスで同じ1本の軸として並べ替えられるため、スプライトと文字の
    // 前後はここの数値だけで決まり、Systemの登録順には依存しない。
    //
    // 帯を100刻み・帯の中を10刻みにしてあるのは、後から1枚挟むたびに
    // 全体を振り直さずに済むようにするため。既定値0は「層を指定し忘れた」
    // 状態で最下層より更に奥へ落ちる。新しいUIは必ずここへ層を足すこと
    //-------------------------------------------------------------

    //! ワールド上の対象に紐づくUI（WorldAnchorComponentで画面へ投影されるもの）
    constexpr int kEnemyHpBarBackground = 100;    //!< 敵の頭上HPバー・背景
    constexpr int kEnemyHpBarFill       = 101;    //!< 敵の頭上HPバー・残量
    constexpr int kDamageNumber         = 110;    //!< ダメージ数値
    constexpr int kPickupPrompt         = 120;    //!< 拾得プロンプト。操作の案内なので
                                                   //!< 飛び交うダメージ数値に隠されないよう最前面に置く

    //-------------------------------------------------------------
    //! 操作プロンプト（キーキャップ・マウス・矢印・長押しゲージ）の帯。
    //! 1つのプロンプトが ベース+0 〜 +4 の5層を内側で使い分ける：
    //!   +0 長押しゲージのセグメント（未点灯・点灯とも同じ層で、色だけ変える）
    //!   +1 予備
    //!   +2 キーキャップの外枠 / マウス本体
    //!   +3 キーキャップの面 / マウスの左ボタン / 矢印
    //!   +4 キーの文字・対象名
    //! InputPromptWidgetがこのオフセットを前提にしているので、間隔を詰めないこと
    //-------------------------------------------------------------
    constexpr int kInputPromptBase = 121;    //!< ワールド追従のプロンプト（拾う・溜め攻撃）

    //! 画面固定のHUD
    constexpr int kHudBarBackground = 200;    //!< HP/EXPバーの背景
    constexpr int kHudBarFill       = 201;    //!< HP/EXPバーの残量
    constexpr int kHudSkillIcon     = 202;    //!< EXPバーの下に並ぶ取得済みスキルのアイコン枠
    constexpr int kHudText          = 210;    //!< HP/EXPの数値・生存時間・取得済みスキル名

    //! 画面右の取得ログ（武器取得・レベルアップ等が右からスライドインして上へ消える）。
    //! スキル選択の暗転板(400)より奥なので、レベルアップメニュー表示中はモーダルの下に沈む
    constexpr int kGameLogPanel  = 220;    //!< 1行の黒い半透明パネル
    constexpr int kGameLogAccent = 221;    //!< パネル左端の種別色バー
    constexpr int kGameLogText   = 230;    //!< 種別ラベル・主題テキスト

    //! 画面下の操作の案内（タイトルから始めたときだけ）。HUDと同じく、スキル選択の暗転板(400)より奥
    constexpr int kTutorialPanel      = 240;    //!< 黒い半透明の板
    constexpr int kTutorialText       = 250;    //!< 案内の文・「操作 n / 6」
    constexpr int kTutorialPromptBase = 255;    //!< キー表示（+4までの5層を使う）

    //! 全画面演出
    constexpr int kScreenDamageFlash = 300;    //!< 被弾時の赤フラッシュ。HUDより手前に掛けて画面全体を染める

    //! モーダル：スキル選択（レベルアップ）
    constexpr int kSkillSelectBackdrop  = 400;    //!< 画面全体の暗転板。ここより奥は全て沈む
    constexpr int kSkillSelectHighlight = 410;    //!< 選択中カードの強調枠（カードの奥に敷いて縁に見せる）
    constexpr int kSkillSelectCard      = 420;    //!< カードの背景パネル
    constexpr int kSkillSelectText      = 430;    //!< タイトル・スキル名・説明文

    //! モーダルの上に重ねる操作プロンプト。カード(420)・説明文(430)より手前に置かないと
    //! 暗転板ではなくカード自身の下に沈む。kInputPromptBaseと同じく+4までの5層を使う
    constexpr int kModalInputPromptBase = 440;

    //! モーダル：リザルト（GAME OVER / CLEAR）
    //! スキル選択より手前に置く。レベルアップの予約と走行の終わりが同じフレームに重なっても、
    //! リザルトが暗転板の下に沈んで「操作が効かないのに理由が分からない」状態にならないようにする
    constexpr int kRunResultBackdrop = 500;    //!< 画面全体の暗転板
    constexpr int kRunResultText     = 510;    //!< 見出し・成績・ベスト記録
    constexpr int kRunResultMenuBase = 520;    //!< リトライ／タイトルへ（GameMenuWidgetが+0〜+24を使う）

    //! モーダル：ポーズ。走行中にしか開けないのでリザルトとは重ならないが、
    //! 念のため全てのモーダルより手前に置く
    constexpr int kPauseBackdrop = 600;    //!< 画面全体の暗転板
    constexpr int kPauseText     = 610;    //!< 「PAUSE」の見出し
    constexpr int kPauseMenuBase = 620;    //!< 再開／オプション／リトライ／タイトルへ（GameMenuWidgetが+0〜+24を使う）
    constexpr int kPauseOptionsBase = 700;    //!< ポーズから開くオプション画面（OptionsMenuが+0〜+44を使う）。ポーズの暗転板より手前

    //! タイトル画面（TitleScene。戦闘シーンとは別のシーンなので上の帯とは衝突しない）
    constexpr int kTitleBackdrop      = 100;    //!< 背景の板
    constexpr int kTitleText          = 110;    //!< タイトル・副題・ベスト記録
    constexpr int kTitleMenuBase      = 120;    //!< はじめる／操作説明／終了（GameMenuWidgetが+0〜+24を使う）
    constexpr int kTitleControlsPanel = 200;    //!< 操作説明の板
    constexpr int kTitleControlsText  = 210;    //!< 操作説明の文字
    constexpr int kTitleControlsMenuBase = 220;    //!< 操作説明の「もどる」
    constexpr int kTitleOptionsBase      = 300;    //!< タイトルから開くオプション画面（OptionsMenuが+0〜+44を使う）

    //! ロード画面（LoadingScene。これも別シーンなので他の帯とは衝突しない）
    constexpr int kLoadingBackdrop = 100;    //!< 背景の板
    constexpr int kLoadingParts    = 110;    //!< 進捗バーの溝(+0)・中身(+1)、回る印
    constexpr int kLoadingText     = 120;    //!< 「NOW LOADING」・進捗の数字

    //! デバッグHUD。調査用なので常に全ての演出より手前に出す
    //! （既定値0のままだと暗転板やフラッシュの下へ沈む）
    constexpr int kDebugWeaponGripHud  = 900;    //!< 武器の握り位置調整HUD（F6）
    constexpr int kDebugStressTestHud  = 901;    //!< 負荷試験HUD（F1）
    constexpr int kDebugWeaponLevelHud = 902;    //!< 所持武器のレベル表示HUD（常時表示。_DEBUGビルドのみ）

    //-------------------------------------------------------------
    // ここから下は RenderPass::World（上とは別の軸）。
    // SpriteSpace::World のスプライトは World パスへ積まれ、Renderer は
    // このパスを並べ替えない。順序を決めているのは SpriteRenderSystem の
    // ローカルソートだけで、前後は基本的に深度バッファが決める
    //-------------------------------------------------------------
    namespace World {
        constexpr int kExpOrb = 15;    //!< EXP玉（ビルボード）
    }
}    // namespace CombatAndroid::UI
