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
    };
}    // namespace CombatAndroid::ECS
