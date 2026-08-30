//-------------------------------------------------------------
//! @file   PlayerSkillHudSystem.hpp
//! @brief  PlayerSkillHudSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  PlayerSkillHudSystem
    //! @brief  画面左のHP/EXPバーの下へ、取得済みスキルを
    //!         「アイコン枠＋スキル名＋Lv」の1行として縦に並べるシステム。
    //!         PlayerHudSystemと同じく固定ピクセル座標のスプライト・テキストを直接書き換える。
    //!         表示する内容の実体はPlayerSkillComponent::levelsで、こちらは表示だけを受け持つ
    //-------------------------------------------------------------
    class PlayerSkillHudSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
