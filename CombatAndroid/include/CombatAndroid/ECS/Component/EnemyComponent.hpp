//-------------------------------------------------------------
//! @file   EnemyComponent.hpp
//! @brief  EnemyComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>
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
        // ノックバック。CombatHit（RequestKnockback）が要求を立て、BT（PlayKnockback）が
        // それを消費してisKnockedBackへ進む。
        // 硬直中（isKnockedBack）は「今より強い要求」だけが上書きできる（knockbackStrengthで比較）。
        // 弱い要求を弾くことで連撃で仰け反り続けるハメを防ぎつつ、3段目の吹っ飛ばしは
        // 1・2段目の怯み中の敵にもきちんと通るようにしている
        //-------------------------------------------------------------
        float knockbackDamageThreshold = 40.0f;    //!< この値以上の単発ダメージを受けたらノックバックする。
                                                     //!< 重い武器（WeaponComponent::knockbackIgnoresThreshold）はこれを無視して必ず発動させる
        bool  pendingKnockback           = false;    //!< ノックバック開始待ち（RequestKnockbackが立て、BTのPlayKnockbackが消費する）
        bool  isKnockedBack              = false;    //!< 硬直中か

        //-------------------------------------------------------------
        // 吹っ飛ばし（位置の押し出し）。従来ののけぞりは「その場硬直」だけだったが、
        // 重い武器の一撃・3段目のAoEでは実際に敵を後ろへ滑らせる。
        // 押し出しはPlayKnockbackがTransformへ直接積む（敵はKinematic+センサーカプセルで
        // Transformに追従するだけなので、物理へインパルスを与える経路は無い）
        //-------------------------------------------------------------
        hlslpp::float3 knockbackVelocity  = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 水平の押し出し速度（ユニット/秒）。毎フレーム指数減衰する
        float          knockbackDecayRate = 8.0f;                                  //!< 上記の指数減衰の速さ（1/秒）。到達距離 ≒ 初速 / この値
        float          knockbackStrength  = 0.0f;                                  //!< 現在発動中のノックバックの初速。より強い要求だけが硬直中でも上書きできる
        float          stunTimer           = 0.0f;                                  //!< 残り強制硬直時間（秒）。0より大きい間はKnockback分岐から抜けない

        //-------------------------------------------------------------
        // RequestKnockbackが書き、PlayKnockbackの立ち上がり1回だけが消費する要求内容
        //-------------------------------------------------------------
        hlslpp::float3 pendingKnockbackVelocity = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 押し出しの初速ベクトル（水平）
        float          pendingKnockbackStun     = 0.0f;                                  //!< 追加の強制硬直時間（秒）

        //-------------------------------------------------------------
        // 死亡演出（Stunned再生 → フェードアウト → 破棄）の進行状態。BT（PlayDeath）が管理する
        //-------------------------------------------------------------
        bool  isDeathAnimFinished = false;    //!< Stunnedを再生しきったか（フェードアウト開始のトリガ）
        float deathFadeDuration   = 0.8f;      //!< フェードアウトにかける時間（秒）
        float deathFadeTimer      = 0.0f;      //!< フェードアウト開始からの経過時間
    };
}    // namespace CombatAndroid::ECS
