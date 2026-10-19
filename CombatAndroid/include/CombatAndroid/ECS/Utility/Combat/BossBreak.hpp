//-------------------------------------------------------------
//! @file    BossBreak.hpp
//! @brief   ボス戦の駆け引き（ブレイク・ジャスト回避と反撃・後隙ボーナス）の共通処理の宣言
//! @note    ボスへの当たりはApplyCombatHit（CombatHit.hpp）、ボスの攻撃の被弾はDamagePlayerから呼ぶ。
//!          調整値は Assets/Tables/Systems/BossCombat.json、ボスごとの体勢ゲージは Assets/Tables/Bosses/<名前>.json の break
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct PlayerComponent;

    //-------------------------------------------------------------
    //! @struct BossCombatParams
    //! @brief  駆け引きの調整値（Assets/Tables/Systems/BossCombat.json。ここの初期値はJSONにキーが無いときの既定値）
    //-------------------------------------------------------------
    struct BossCombatParams {
        float justWindow          = 0.15f;    //!< 回避を始めてからこの秒数以内に敵の攻撃をすり抜けたらジャスト回避
        float regularSlowCooldown = 4.0f;     //!< 雑魚でジャスト回避したときのスローを、次に掛けられるまでの秒数（ボスは毎回掛ける）
        float counterWindow       = 1.5f;     //!< ジャスト回避の後、反撃の構えが続く秒数
        float counterDamageScale  = 2.5f;     //!< 反撃の一撃のダメージ倍率
        float counterBreakScale   = 4.0f;     //!< 反撃の一撃が体勢ゲージを削る倍率
        float recoveryDamageScale = 1.5f;     //!< 技の後の隙に当てたときのダメージ倍率
        float recoveryBreakScale  = 2.0f;     //!< 技の後の隙に当てたときに体勢ゲージを削る倍率
        float downDamageScale     = 2.0f;     //!< ダウン中に当てたときのダメージ倍率
        float heavyBreakScale     = 2.0f;     //!< 重い一撃（溜め解放の斬撃弾・連撃3段目の範囲攻撃）が体勢ゲージを削る倍率

        hlslpp::float3 counterRimColor = hlslpp::float3(1.0f, 0.9f, 0.6f);    //!< 反撃の構えの間、プレイヤーを縁取る色
        float          counterRimIntensity = 4.0f;                            //!< 縁取りの強さ

        // ボスの隙（技の後・ダウン中）をボスの体の金色の縁取りで見せる（EnemyAttackTelegraphSystem）
        hlslpp::float3 vulnerableRimColor     = hlslpp::float3(1.0f, 0.78f, 0.25f);    //!< 縁取りの色
        float          vulnerableRimIntensity = 3.0f;                                  //!< 技の後の隙での縁取りの強さ（脈打ちの山）
        float          downRimIntensity       = 5.0f;                                  //!< ダウン中の縁取りの強さ（脈打ちの山）
        float          vulnerableGlow         = 0.15f;                                 //!< 面全体を持ち上げる量（脈打ちの山）
        float          vulnerablePulseSpeed   = 2.5f;                                  //!< 脈打ちの速さ（1秒あたりの回数）
    };

    //-------------------------------------------------------------
    //! @brief  駆け引きの調整値を得る（初回の呼び出しで1度だけ読む）
    //-------------------------------------------------------------
    [[nodiscard]]
    const BossCombatParams& GetBossCombatParams();

    //-------------------------------------------------------------
    //! @brief  敵への一撃に反撃の倍率を掛け、ボスならダウン・後隙の倍率も掛けて体勢ゲージを溜める
    //! @param  registry   [in,out] ECSレジストリ
    //! @param  attacker   [in]     攻撃した側（プレイヤー）。反撃の構えを消費する
    //! @param  hitEntity  [in]     当たった相手。ボス（BossMoveSetComponent）でなければ何もしない
    //! @param  damage     [in]     倍率を掛ける前のダメージ
    //! @param  breakScale [in]     体勢ゲージを削る倍率（重い一撃ならheavyBreakScale）
    //! @return 倍率を掛けた後のダメージ
    //! @note   ゲージが満タンになったらBossMoveSetComponent::breakPendingを立てるだけで、
    //!         ダウンへ移るのはBossBehaviorの次のUpdate
    //-------------------------------------------------------------
    [[nodiscard]]
    float ApplyBossHitModifiers(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity attacker, Tsukino::ECS::Entity hitEntity, float damage,
                                float breakScale);

    //-------------------------------------------------------------
    //! @brief  回避の無敵で敵（ボス・雑魚）の攻撃をすり抜けた瞬間に呼び、ジャスト回避なら反撃の構えを付ける
    //! @param  registry     [in,out] ECSレジストリ
    //! @param  playerEntity [in]     プレイヤー
    //! @param  player       [in,out] プレイヤーの状態（counterReadyTimerを書く）
    //! @param  attacker     [in]     攻撃した側（敵本体。弾なら撃ったボス）
    //! @return ジャスト回避になったか
    //! @note   成立したら PlayerFinisherEvent（インパクト待ち0秒）を流し、大技と同じスローとカメラの寄りを掛ける。
    //!         雑魚のときはPlayerComponent::justDodgeSlowCooldownが残っている間はスローを掛けない（反撃の構えは付く）。
    //!         反撃の構えが残っている間は成立させない（1回の振りの判定区間で何度も成立しないように）
    //-------------------------------------------------------------
    bool TryJustDodge(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity playerEntity, PlayerComponent& player, Tsukino::ECS::Entity attacker);
}    // namespace CombatAndroid::ECS
