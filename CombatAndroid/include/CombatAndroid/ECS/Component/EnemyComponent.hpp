//-------------------------------------------------------------
//! @file   EnemyComponent.hpp
//! @brief  EnemyComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EnemyComponent
    //! @brief  敵エンティティであることを表すコンポーネント。全てBehaviorTreeComponentを持つBT駆動の敵。
    //!         プレイヤーへのダメージは接触ではなく、攻撃モーションに合わせて出る
    //!         EnemyAttackHitboxComponentのカプセル判定（CombatSystem）で行う。
    //!         武器のヒット判定もJolt物理のカプセルセンサー（CollisionComponent、spawnBehaviorEnemyが付与）で行う
    //-------------------------------------------------------------
    struct EnemyComponent {
        float moveSpeed   = 120.0f;    //!< プレイヤーを追跡する速度（1ユニット≒1cm規約）
        float detectRange = 600.0f;    //!< プレイヤーを追跡し始める距離
        float bodyRadius  = 40.0f;     //!< 簡易的な当たり判定半径（武器ヒット判定用カプセルの寸法決めに使用）

        // attackRangeはBTが攻撃へ移る距離であると同時に、MoveToPlayerが足を止める距離でもある
        // （ZombieBehavior参照）。遠すぎると手ボーンの判定球（EnemyAttackHitboxComponent::radius）が
        // プレイヤーのカプセルへ届かず、攻撃モーションが空振りし続けるため、
        // 接触距離（bodyRadius+プレイヤー半径）のすぐ外に置くこと
        float attackRange         = 85.0f;
        float attackCooldown      = 1.5f;      //!< 攻撃を終えてから次の攻撃までの最短間隔（秒）
        float attackCooldownTimer = 0.0f;      //!< 残りクールタイム

        float expReward = 10.0f;    //!< 撃破時にプレイヤーへ与えるEXP量（EnemySpawnConfig::expRewardから設定される）

        //-------------------------------------------------------------
        // ノックバック。CombatSystemが単発ダメージとknockbackDamageThresholdを比較して
        // pendingKnockbackを立て、BT（PlayKnockback）がそれを消費してisKnockedBackへ進む。
        // isKnockedBackの間はpendingKnockbackを立てても無視される（ハメ防止）
        //-------------------------------------------------------------
        float knockbackDamageThreshold = 40.0f;    //!< この値以上の単発ダメージを受けたらノックバックする
        bool  pendingKnockback           = false;    //!< ノックバック開始待ち（CombatSystemが立て、BTのPlayKnockbackが消費する）
        bool  isKnockedBack              = false;    //!< 硬直中か。trueの間は再ノックバックしない

        //-------------------------------------------------------------
        // 死亡演出（Stunned再生 → フェードアウト → 破棄）の進行状態。BT（PlayDeath）が管理する
        //-------------------------------------------------------------
        bool  isDeathAnimFinished = false;    //!< Stunnedを再生しきったか（フェードアウト開始のトリガ）
        float deathFadeDuration   = 0.8f;      //!< フェードアウトにかける時間（秒）
        float deathFadeTimer      = 0.0f;      //!< フェードアウト開始からの経過時間
    };
}    // namespace CombatAndroid::ECS
