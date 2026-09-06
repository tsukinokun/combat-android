//-------------------------------------------------------------
//! @file   InputPromptHudComponent.hpp
//! @brief  InputPromptHudComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/InputPromptWidget.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct InputPromptHudComponent
    //! @brief  操作を促すUI一式を持つHUDエンティティの目印。
    //!         PlayerHudComponentと同じく1つだけ生成して使い回し、
    //!         InputPromptSystemが毎フレーム表示/非表示と値を書き換える。
    //!         ウィジェットが持つのはエンティティのハンドルだけで、
    //!         実際の描画物（スプライト・文字）は別エンティティとして存在する
    //-------------------------------------------------------------
    struct InputPromptHudComponent {
        //! 拾う：[F] ＋ 上向き矢印 ＋ 対象名。武器の頭上にワールド追従で出す
        InputPromptWidget pickupPrompt;

        //! 溜め攻撃：マウスの絵 ＋ 長押しゲージ。プレイヤーの頭上にワールド追従で出す。
        //! 溜めに対応する武器（battleaxe）を持っているときだけ出番がある
        InputPromptWidget chargePrompt;

        //! スキル選択：カーソル上移動の[W]。以下3つは画面固定でモーダルの上に出す
        InputPromptWidget skillUpPrompt;

        //! スキル選択：カーソル下移動の[S]
        InputPromptWidget skillDownPrompt;

        //! スキル選択：決定の[F]
        InputPromptWidget skillConfirmPrompt;

        //! リトライ：[SPACE]。GAME OVER表示中だけ画面固定で出す
        InputPromptWidget retryPrompt;
    };
}    // namespace CombatAndroid::ECS
