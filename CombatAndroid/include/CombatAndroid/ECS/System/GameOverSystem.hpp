//-------------------------------------------------------------
//! @file   GameOverSystem.hpp
//! @brief  GameOverSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  GameOverSystem
    //! @brief  プレイヤーのHealthComponent::isDeadを監視し、死亡モーションの再生が落ち着く頃合いで
    //!         「GAME OVER」表示を出す。その後スペースキーでシーンを作り直し（GameSceneManager::ChangeScene）、
    //!         HP満タンから再開させるシステム
    //-------------------------------------------------------------
    class GameOverSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
