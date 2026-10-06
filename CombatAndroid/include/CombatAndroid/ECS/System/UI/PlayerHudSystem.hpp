//-------------------------------------------------------------
//! @file   PlayerHudSystem.hpp
//! @brief  PlayerHudSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  PlayerHudSystem
    //! @brief  画面左上のプレイヤーHP/EXPバーを、HealthComponent／
    //!         PlayerExperienceComponentの値へ合わせて毎フレーム更新するシステム。
    //!         敵の頭上HPバー（HealthBarSystem）と異なり、WorldAnchorComponentを
    //!         使わず固定ピクセル座標のスプライト・テキストを直接書き換える
    //-------------------------------------------------------------
    class PlayerHudSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
