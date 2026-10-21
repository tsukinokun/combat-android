//-------------------------------------------------------------
//! @file   InputPromptGlyphSystem.hpp
//! @brief  キーキャップの文字と面の色を、最後に触った入力機器に合わせるシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  InputPromptGlyphSystem
    //! @brief  InputPromptGlyphComponentを持つキーキャップの文字を、割り当て（InputBindings.json）と
    //!         最後に触った機器（キーボード・マウス／ゲームパッド）から書き換え、面の色もパッドのボタンの色に合わせる。
    //! @note   位置・大きさ・表示するかは各画面がShowInputPrompt*で決める。ここが変えるのは文字と色だけ。
    //!         マウスの絵で見せている間（キーボード・マウスで左クリックの操作）は、文字を出さないので触らない
    //-------------------------------------------------------------
    class InputPromptGlyphSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
