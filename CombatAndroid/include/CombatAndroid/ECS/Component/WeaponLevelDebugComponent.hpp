//-------------------------------------------------------------
//! @file   WeaponLevelDebugComponent.hpp
//! @brief  WeaponLevelDebugComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct WeaponLevelDebugComponent
    //! @brief  所持武器のレベルを表示するデバッグHUDエンティティの目印。
    //!         WeaponGripDebugComponentと同じく、FontComponentを持つエンティティへ
    //!         付けてWeaponLevelDebugSystemがtextを毎フレーム書き換える。
    //!         コンポーネント自体・利用するSystemともに_DEBUGビルドでのみ存在する
    //!
    //!         空の構造体にしないこと。EnTTは空の型に実体を持たせない最適化を行うため、
    //!         Registry::AddComponent が参照を返せずコンパイルが通らない
    //-------------------------------------------------------------
    struct WeaponLevelDebugComponent {
        bool _unused = true;    //!< 目印としてのみ使う。参照する値は持たない
    };
}    // namespace CombatAndroid::ECS
