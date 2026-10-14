//-------------------------------------------------------------
//! @file   SoftTarget.hpp
//! @brief  ソフトターゲット（自動で狙う敵）と攻撃時の振り向きの共通処理
//! @note   選ぶ側（SoftTargetSystem）・振り向きを要求する側（PlayerAnimationSystem）・
//!         回す側（AttackTurnAssistSystem）が同じ調整値と同じ「基準の向き」を使うよう、ここに1か所だけ置く
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct PlayerComponent;    // 前方宣言

    //-------------------------------------------------------------
    //! @struct SoftTargetParams
    //! @brief  ソフトターゲットの調整値（Assets/Tables/Systems/SoftTarget.json。ここの初期値はJSONにキーが無いときの既定値）
    //! @note   距離はcm（1ユニット=1cm）
    //-------------------------------------------------------------
    struct SoftTargetParams {
        float selectRange    = 600.0f;    //!< ターゲットの候補にする距離（この中なら薄い印が出る）
        float assistRange    = 300.0f;    //!< 攻撃時に振り向く距離（敵の体の表面まで。この中なら濃い印になる）
        float assistAngleDeg = 60.0f;     //!< 基準の向きから何度以内の敵を候補にするか（選ぶのも振り向くのもこの角度の中だけ）
        float angleWeight    = 1.0f;      //!< 候補の点数で、角度のずれをどれだけ重く見るか
        float distanceWeight = 0.6f;      //!< 候補の点数で、距離をどれだけ重く見るか
        float stickiness     = 0.7f;      //!< 今のターゲットの点数に掛ける倍率（小さいほど乗り換えにくい）
        float switchCooldown = 0.15f;     //!< 乗り換えてから次に乗り換えられるまでの秒数（印のちらつき防止）
        float turnDuration   = 0.06f;     //!< 攻撃時に振り向くのにかける秒数（0で瞬時）
    };

    //-------------------------------------------------------------
    //! @brief  調整値を得る（初回の呼び出しで1度だけ読む）
    //-------------------------------------------------------------
    [[nodiscard]]
    const SoftTargetParams& GetSoftTargetParams();

    //-------------------------------------------------------------
    //! @brief  ターゲットを選ぶ・振り向くときの「基準の向き」を求める
    //! @param  player   [in] プレイヤー（移動入力の向きを読む）
    //! @param  rotation [in] プレイヤーの今の姿勢
    //! @return 水平・正規化済みの向き。移動入力があればその向き、無ければキャラの正面（ローカル+Z）
    //-------------------------------------------------------------
    [[nodiscard]]
    hlslpp::float3 ResolveAimDirection(const PlayerComponent& player, const hlslpp::quaternion& rotation);

    //-------------------------------------------------------------
    //! @brief  攻撃に入った瞬間に、ターゲットの方向への振り向きを要求する
    //! @param  registry [in,out] ECSレジストリ
    //! @param  player   [in]     プレイヤーのエンティティ（SoftTargetComponentとTransformComponentが必要）
    //! @note   ターゲットが振り向きの効く範囲（assistReady）にいるときだけ要求する。
    //!         実際に回すのはAttackTurnAssistSystem（攻撃の段に入ったのと同じフレームから回り始める）
    //-------------------------------------------------------------
    void RequestAttackTurnAssist(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity player);
}    // namespace CombatAndroid::ECS
