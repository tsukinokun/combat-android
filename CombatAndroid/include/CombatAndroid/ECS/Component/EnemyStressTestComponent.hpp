//-------------------------------------------------------------
//! @file    EnemyStressTestComponent.hpp
//! @brief   EnemyStressTestComponent構造体の宣言
//! @author  山﨑愛
//-------------------------------------------------------------
#pragma once

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct EnemyStressTestComponent
    //! @brief  負荷試験のHUDを表示するエンティティの目印
    //! @note   WeaponGripDebugComponentと同じく、FontComponentを持つエンティティへ
    //!         付けてEnemyStressTestSystemがtextを毎フレーム書き換える。
    //!         敵数や配置といった試験そのものの状態はSystem側が持ち、
    //!         ここにはHUDの表示状態だけを置く。
    //!
    //!         空の構造体にしないこと。EnTTは空の型に実体を持たせない最適化を行うため、
    //!         Registry::AddComponent が参照を返せずコンパイルが通らない
    //-------------------------------------------------------------
    struct EnemyStressTestComponent {
#ifdef _DEBUG
        bool visible = true;     //!< HUDを表示するか（F2でトグル）。Debugでは起動直後から表示
#else
        bool visible = false;    //!< Releaseでは自動表示せず、F2キーを押した場合のみ表示（負荷試験自体は引き続き動作）
#endif
    };
}    // namespace CombatAndroid::ECS
