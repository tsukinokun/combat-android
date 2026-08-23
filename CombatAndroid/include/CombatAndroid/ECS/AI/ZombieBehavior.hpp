//-------------------------------------------------------------
//! @file   ZombieBehavior.hpp
//! @brief  ゾンビ系敵（SmallZombie/BigZombie共通）用ビヘイビアツリー構築関数の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/BehaviorTreeComponent.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  ゾンビ系敵用のビヘイビアツリーを構築する。
    //!         射程・速度・ダメージ閾値・使用クリップは全てコンポーネント側の値なので、
    //!         SmallZombie・BigZombieいずれもこの1種類のツリーを共有できる
    //! @details
    //! Selector（記憶あり）
    //!  ├─ Sequence "Death"     : 死亡していればStunnedを再生し、フェードアウトして消える（最優先）
    //!  ├─ Sequence "Knockback" : ノックバック待ち/硬直中ならZombie Reaction Hitでその場に硬直する
    //!  ├─ Sequence "Attack"    : 攻撃射程内 かつ クールタイム明け なら攻撃を再生する
    //!  ├─ Sequence "Chase"     : 索敵範囲内ならプレイヤーへ近づく
    //!  └─ Action   "Idle"      : どれも成立しなければ待機する
    //! @return 構築済みツリーのルートノード
    //-------------------------------------------------------------
    std::shared_ptr<EnemyBehaviorNode> BuildZombieTree();
}    // namespace CombatAndroid::ECS
