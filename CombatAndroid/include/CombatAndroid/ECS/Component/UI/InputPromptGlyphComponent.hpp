//-------------------------------------------------------------
//! @file    InputPromptGlyphComponent.hpp
//! @brief   操作を指定したキーキャップの文字に付ける印
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/Input/GameInput.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct InputPromptGlyphComponent
    //! @brief  キーキャップの文字（FontComponentを持つエンティティ）に付け、案内する操作を覚えておく
    //! @note   CreateInputPromptWidgetがInputPromptDesc::actionを指定されたときに付ける。
    //!         InputPromptGlyphSystemが、最後に触った機器に合わせて文字と面の色を毎フレーム書き換える
    //!         （メニューのように、開いたときや選択が動いたときしか組み直さない画面でも切り替わるように）
    //-------------------------------------------------------------
    struct InputPromptGlyphComponent {
        GameAction           action     = GameAction::Count;    //!< 案内する操作
        Tsukino::ECS::Entity faceEntity = entt::null;           //!< キーキャップの面（パッドのボタンの色に塗る）
    };
}    // namespace CombatAndroid::ECS
