//-------------------------------------------------------------
//! @file    CombatHit.hpp
//! @brief   敵1体へのヒットを確定させる共通処理の宣言
//! @author  山﨑愛
//! @note    武器のスイング（CombatSystem）と斬撃弾（ProjectileSystem）の両方から
//!          同じ経路を通す。ここを共有しないと、多重ヒット防止・ノックバック閾値・
//!          嫉妬の吸収上限・ヒットストップのどれか1つを片方だけ直す事故が起きる
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>

#include <hlsl++.h>

#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    // ヒットストップの調整用定数（実機で見た目を確認しながら調整する）
    //-------------------------------------------------------------
    inline constexpr float kHitStopDuration = 0.08f;    //!< ヒット時にかかる減速の持続時間（実時間・秒）
    inline constexpr float kHitStopScale    = 0.02f;    //!< 持続時間中のdeltaTimeへのスケール値（小さいほど強い停止）

    inline constexpr float kHpBarVisibleDuration = 3.0f;    //!< 被弾時に頭上HPバーを表示し続ける時間（秒）。HealthBarSystemが減算する

    //-------------------------------------------------------------
    //! スキル「嫉妬」が1回のアタックで吸収できるHPの上限（最大HPに対する割合）。
    //!
    //! 吸収量は与ダメージ比例なので、AoE対応武器（warhammerの3段目）や貫通する斬撃弾で
    //! 群れを巻き込むとヒット数ぶん青天井に伸びてしまう。ヒット1発ずつに上限を付けても
    //! 巻き込み数で総量が伸びるのは変わらないため、アタック単位の総量で頭打ちにする
    //-------------------------------------------------------------
    inline constexpr float kLifeStealCapRatioPerAttack = 0.25f;

    //-------------------------------------------------------------
    //! @struct KnockbackParams
    //! @brief  ノックバック（のけぞり＋吹っ飛ばし）1回ぶんの要求内容
    //! @note   既定構築（全て0/false）なら「ダメージがknockbackDamageThresholdを超えたときに
    //!         その場で怯むだけ」という従来どおりの挙動になる。斬撃弾（ProjectileSystem）は
    //!         これをそのまま渡している
    //-------------------------------------------------------------
    struct KnockbackParams {
        hlslpp::float3 sourcePosition = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 押し出しの起点。ここから対象へ向かう水平方向へ押し出す
        float          speed                 = 0.0f;     //!< 押し出しの初速（ユニット/秒）。0なら位置を動かさず怯むだけ
        float          stunDuration          = 0.0f;     //!< 追加の強制硬直時間（秒）。EnemyAnimationSetComponent::knockbackTimeoutSafetyを超えさせないこと
        bool           ignoreDamageThreshold = false;    //!< trueならknockbackDamageThresholdを満たさなくても発動する（重い武器用）
    };

    //-------------------------------------------------------------
    //! @brief  敵1体へノックバックを要求する
    //! @param  registry     [in,out] ECSレジストリ
    //! @param  enemyEntity  [in]     対象。EnemyComponentとTransformComponentを持たなければ何もしない
    //! @param  params       [in]     要求内容
    //! @note   硬直中（isKnockedBack）は「今より強い要求」だけが上書きできる。
    //!         弱い要求を弾くことで連撃で仰け反り続けるハメを防ぎつつ、3段目の吹っ飛ばしは
    //!         怯み中の敵にも通る。同じ内容で2回呼んでも2回目は上書き条件を満たさず無害
    //!         （AoEがダメージ経路と押し出し経路の両方から呼ぶため、これに依存している）
    //-------------------------------------------------------------
    void RequestKnockback(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity enemyEntity, const KnockbackParams& params);

    //-------------------------------------------------------------
    //! @brief  1体のエンティティへヒットストップを要求/更新する。
    //!         画面全体ではなく対象エンティティだけをHitStopSystemが減速させる
    //! @param  registry [in,out] ECSレジストリ
    //! @param  entity   [in] 対象エンティティ（プレイヤーまたは敵）。entt::nullなら何もしない
    //! @param  duration [in] 持続時間（実時間・秒）
    //! @param  scale    [in] 持続時間中、対象エンティティのアニメーション/移動へ掛けるスケール値
    //-------------------------------------------------------------
    void ApplyHitStop(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float duration, float scale);

    //-------------------------------------------------------------
    //! @brief  1体の敵へのヒットを確定させる共通処理。
    //!         多重ヒット防止・ダメージ・ノックバック要求・嫉妬の吸収・WeaponHitEventの発火・
    //!         ヒットストップ要求を一本化する
    //! @param  registry            [in,out] ECSレジストリ
    //! @param  eventBus            [in]     ヒット通知の発行先（nullptrなら発行しない）
    //! @param  attacker            [in]     攻撃した側（プレイヤー）。ヒットストップと嫉妬の回復先
    //! @param  sourceEntity        [in]     WeaponHitEvent::weaponへ載せる実体（武器エンティティ／弾エンティティ）
    //! @param  hitEntity           [in]     ヒット対象。敵でない・死亡済みなら何もしない
    //! @param  dealtDamage         [in]     与える実ダメージ（倍率適用後の確定値）
    //! @param  hitPositionFallback [in]     対象にTransformComponentが無い場合に使う位置
    //! @param  hitRecord           [in,out] 多重ヒット防止の記録。既に載っている相手はスキップし、当てたら追加する
    //! @param  lifeStealRatio      [in]     スキル「嫉妬」の吸収割合（0なら吸収しない）
    //! @param  lifeStealHealed     [in,out] 上記の吸収済み総量。上限（kLifeStealCapRatioPerAttack）の判定に使う
    //! @param  knockback           [in]     ノックバックの要求内容。既定構築なら従来どおり（閾値超えでその場硬直）
    //! @return 実際にダメージを与えたらtrue
    //-------------------------------------------------------------
    bool ApplyCombatHit(Tsukino::ECS::Registry& registry,
                        Tsukino::ECS::EventBus* eventBus,
                        Tsukino::ECS::Entity attacker,
                        Tsukino::ECS::Entity sourceEntity,
                        Tsukino::ECS::Entity hitEntity,
                        float dealtDamage,
                        const hlslpp::float3& hitPositionFallback,
                        std::vector<Tsukino::ECS::Entity>& hitRecord,
                        float lifeStealRatio,
                        float& lifeStealHealed,
                        const KnockbackParams& knockback = KnockbackParams{});
}    // namespace CombatAndroid::ECS
