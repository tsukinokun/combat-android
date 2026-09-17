//-------------------------------------------------------------
//! @file    GameplayFreeze.hpp
//! @brief   ゲームの進行を止める（メニュー表示中）判定と、止めている間の後始末の宣言
//! @note    進行を止める画面は3つある：スキル選択・ポーズ・リザルト。
//!          止めている間はシーンがdeltaTimeを0にし、入力を読むSystem群は早期リターンする。
//!          以前はどのSystemもIsSkillSelectActiveを直接見ていたが、画面が増えるたびに
//!          全箇所へ条件を足すと漏れるので、ここへ1本化した
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Registry/Registry.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  今ゲームの進行を止めているかを問い合わせる
    //! @param  registry [in] ECSレジストリ
    //! @return true: スキル選択・ポーズ・リザルトのどれかが進行を止めている
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsGameplayFrozen(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @brief  全キャラクタの移動入力を打ち消す
    //! @param  registry [in] ECSレジストリ
    //! @note   PhysicsSystemはdeltaTimeが0以下でも1/60秒ぶん必ずステップする。
    //!         そのためシーン側でdeltaTime=0にしても、moveInputが残っていると
    //!         CharacterVirtualは毎フレーム動き続けてしまう。移動入力を書くSystem群は
    //!         停止中は早期リターンして何も書かないので、ここで潰した値が保たれる
    //-------------------------------------------------------------
    void SuppressAllMoveInput(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @brief  進行中のヒットストップ（HitStopComponent）を全エンティティから取り除く
    //! @param  registry [in] ECSレジストリ
    //! @note   停止中はSceneへ渡すdeltaTimeが0になりHitStopSystemの減算処理が止まるため、
    //!         放っておくと停止を解いた後にスローモーションが残ってしまう。
    //!         コンポーネント構成を変えるので、Viewの反復の外側から呼ぶこと
    //-------------------------------------------------------------
    void ClearAllHitStop(Tsukino::ECS::Registry& registry);
}    // namespace CombatAndroid::ECS
