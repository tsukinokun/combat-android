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
    constexpr int kPickupPrompt         = 120;    //!< 「Fキーで拾う」。操作の案内なので
                                                   //!< 飛び交うダメージ数値に隠されないよう最前面に置く

    //! 画面固定のHUD
    constexpr int kHudBarBackground = 200;    //!< HP/EXPバーの背景
    constexpr int kHudBarFill       = 201;    //!< HP/EXPバーの残量
    constexpr int kHudText          = 210;    //!< HP/EXPの数値・生存時間

    //! 全画面演出
    constexpr int kScreenDamageFlash = 300;    //!< 被弾時の赤フラッシュ。HUDより手前に掛けて画面全体を染める

    //! モーダル：スキル選択（レベルアップ）
    constexpr int kSkillSelectBackdrop  = 400;    //!< 画面全体の暗転板。ここより奥は全て沈む
    constexpr int kSkillSelectHighlight = 410;    //!< 選択中カードの強調枠（カードの奥に敷いて縁に見せる）
    constexpr int kSkillSelectCard      = 420;    //!< カードの背景パネル
    constexpr int kSkillSelectText      = 430;    //!< タイトル・スキル名・説明文

    //! モーダル：GAME OVER
    //! スキル選択より手前に置く。レベルアップの予約と死亡が同じフレームに重なると
    //! 両方のUIが立ち得るため、GAME OVERが暗転板の下に沈んで「操作が効かないのに
    //! 理由が分からない」状態になるのを避ける
    constexpr int kGameOverText = 500;

    //! デバッグHUD。調査用なので常に全ての演出より手前に出す
    //! （既定値0のままだと暗転板やフラッシュの下へ沈む）
    constexpr int kDebugWeaponGripHud = 900;    //!< 武器の握り位置調整HUD（F6）
    constexpr int kDebugStressTestHud = 901;    //!< 負荷試験HUD（F1）

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
