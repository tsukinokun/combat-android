//-------------------------------------------------------------
//! @file   HealthComponent.hpp
//! @brief  HealthComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct HealthComponent
    //! @brief  HP（体力）を持つエンティティに付与するコンポーネント
    //-------------------------------------------------------------
    struct HealthComponent {
        float maxHealth     = 100.0f;    //!< 最大HP
        float currentHealth = 100.0f;    //!< 現在HP
        bool  isDead         = false;    //!< HPが尽きたか（実際のエンティティ破棄は各Systemが行う）

        // --- 頭上HPバー（HealthBarSystemが使用。無い場合はentt::nullのまま） ---
        Tsukino::ECS::Entity hpBarBackgroundEntity = entt::null;    //!< HPバー背景エンティティ
        Tsukino::ECS::Entity hpBarFillEntity       = entt::null;    //!< HPバー残量エンティティ
        Tsukino::ECS::Entity hpBarTrailEntity      = entt::null;    //!< HPバーの削れエンティティ（減った分を遅れて縮める）
        float                hpBarVisibleTimer     = 0.0f;          //!< 0より大きい間だけHPバーを表示する残り時間（秒）。被弾時にCombatSystemがセットする
        float                hpBarTrailRatio       = 1.0f;          //!< 削れのバーが今示している割合（0〜1）
        float                hpBarLastRatio        = 1.0f;          //!< 前のフレームの残量の割合（減った瞬間を見分けるため）
        float                hpBarTrailDelay       = 0.0f;          //!< 削れが縮み始めるまでの残り秒
        float                hpBarFlashTimer       = 0.0f;          //!< 残量のバーを白く光らせる残り秒（減った瞬間に立てる）
    };
}    // namespace CombatAndroid::ECS
