//-------------------------------------------------------------
//! @file   BossMoveSetComponent.hpp
//! @brief  BossMoveSetComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Engine/Asset/AssetHandle.hpp>

#include <hlsl++.h>

#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum  BossActionPhase
    //! @brief ボスが今していること
    //-------------------------------------------------------------
    enum class BossActionPhase : int {
        Chase = 0,    //!< 追いかける（技を選べるのを待つ）
        Move,         //!< 技を出している（予兆 → 判定 → 終わり）
        Recovery,     //!< 技の後の隙
        Broken,       //!< 体勢ゲージが満タンになってダウンしている（技を出さず、受けるダメージが増える）
        Transform,    //!< 第二形態へ変身している（崩れ落ちてから浮き上がって大きくなる。この間はダメージを受けない）
    };

    //-------------------------------------------------------------
    //! @struct BossMoveSetComponent
    //! @brief  複数の技を持つボスの状態。技の中身は BossMoveTable（Assets/Tables/Bosses/<key>.json）にあり、
    //!         ここは「今どの技をどこまで出したか」だけを持つ
    //! @note   BossDirectorSystemが湧かせたときに付け、ボスのビヘイビアツリー（BossBehavior）だけが書き込む。
    //!         EnemyAttackAreaSystemが予兆の形と位置を読む
    //-------------------------------------------------------------
    struct BossMoveSetComponent {
        std::string key;    //!< 技の表の名前（例："Vampire"）

        std::vector<Tsukino::Asset::AssetHandle> clips;        //!< 技ごとのクリップ（湧かせたときに読んでおく）
        std::vector<float>                       cooldowns;    //!< 技ごとの残りクールタイム（秒）
        Tsukino::Asset::AssetHandle              breakClip;    //!< ダウン中のクリップ（湧かせたときに読んでおく）。無効ならのけぞりのクリップ

        float sizeScale   = 1.0f;    //!< 判定の半径へ掛ける倍率（ボスの大きさ）
        float damageScale = 1.0f;    //!< ダメージへ掛ける倍率（危険度・ボスの攻撃力）

        BossActionPhase phase       = BossActionPhase::Chase;
        int             currentMove = -1;      //!< 出している技の番号。-1なら無し
        int             repeatLeft  = 0;       //!< 同じ技をあと何回続けるか（激昂中の連続攻撃）
        float           phaseTimer  = 0.0f;    //!< 今の段階に入ってからの秒
        bool            summoned    = false;   //!< この技で雑魚をもう呼んだか（弾ならもう撃ったか）
        bool            chargeLocked = false;  //!< 突進の向きをもう固定したか
        hlslpp::float3  chargeDirection{0.0f, 0.0f, 1.0f};    //!< 突進の向き（駆け出した瞬間に固定）
        float           orbitSign   = 1.0f;    //!< 旋回の向き（1で反時計回り、-1で時計回り）

        hlslpp::float3 moveOrigin{0.0f, 0.0f, 0.0f};    //!< 技を始めた位置（跳びかかりの踏み切り）
        hlslpp::float3 moveTarget{0.0f, 0.0f, 0.0f};    //!< 技の狙い（跳びかかりの着地点）

        bool  enraged          = false;    //!< 激昂したか
        bool  enrageMovePending = false;   //!< 激昂の合図の技をまだ出していないか
        float baseMoveSpeed    = 0.0f;     //!< 激昂前の移動速度（湧かせたときの値）

        // --- ブレイク（体勢崩し）。溜めるのはApplyBossHitModifiers（BossBreak.hpp）、ダウンさせるのはBossBehavior ---
        float breakGauge       = 0.0f;     //!< 今の体勢ゲージ（0〜breakGaugeMax）
        float breakGaugeMax    = 0.0f;     //!< 体勢ゲージの最大値。0なら初回のUpdateで表から決める
        float breakIdleTimer   = 0.0f;     //!< 最後に当てられてからの秒（一定時間たつとゲージが減り始める）
        int   breakCount       = 0;        //!< ダウンした回数（回数ごとに最大値を増やす）
        bool  breakPending     = false;    //!< ゲージが満タンになった。次のUpdateでダウンへ移る

        // --- 第二形態。HPが0になったとき CombatHit が transformPending を立て、BossBehavior が変身させる ---
        bool  secondFormUsed     = false;    //!< もう変身したか（変身は1回だけ）
        bool  transformPending   = false;    //!< HPが0になった。次のUpdateで変身を始める
        bool  transformAnnounced = false;    //!< 変身の告知（名前の帯・カメラ）をBossDirectorSystemが出したか
        float transformFromScale = 1.0f;     //!< 変身前の見た目の拡大率（ここから倍率まで大きくする）
        float pendingColliderScale = 1.0f;   //!< 当たり判定のカプセルへまだ掛けていない倍率（BossDirectorSystemがViewの外で掛けて1へ戻す）

        // --- 空を飛ぶボス。BossBehavior が毎フレーム高さを目標へ寄せる ---
        float altitude    = 0.0f;     //!< 地面からの今の高さ（揺れを除く）
        float groundY     = 0.0f;     //!< 地面の高さ（湧いた位置の高さ）
        bool  groundKnown = false;    //!< groundYを取ったか
        float bobTime     = 0.0f;     //!< 上下の揺れの経過秒
    };
}    // namespace CombatAndroid::ECS
