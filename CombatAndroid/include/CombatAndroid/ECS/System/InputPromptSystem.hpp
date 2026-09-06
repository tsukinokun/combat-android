//-------------------------------------------------------------
//! @file   InputPromptSystem.hpp
//! @brief  InputPromptSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  InputPromptSystem
    //! @brief  「今なにを押せばいいか」を文字ではなく図で示すUIを毎フレーム更新するシステム。
    //!         拾う(F) / 溜め攻撃(左クリック長押し) / スキル選択(W・S・F) / リトライ(SPACE)の
    //!         4箇所を1本で面倒を見る。表示の判断があちこちのSystemへ散ると
    //!         「メニュー中なのに拾得プロンプトが残る」類の取りこぼしが起きるため、
    //!         PlayerHudSystemと同じくUIの都合はUI側のSystemへ集約している。
    //!         実際の絵の組み立ては ECS/Utility/InputPromptWidget が持つ
    //-------------------------------------------------------------
    class InputPromptSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
