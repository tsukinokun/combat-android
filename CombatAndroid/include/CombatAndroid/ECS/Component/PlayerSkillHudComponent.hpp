//-------------------------------------------------------------
//! @file   PlayerSkillHudComponent.hpp
//! @brief  PlayerSkillHudComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <array>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerSkillHudRow
    //! @brief  取得済みスキル一覧の1行ぶんのエンティティ参照
    //-------------------------------------------------------------
    struct PlayerSkillHudRow {
        //! アイコン枠のスプライト。スキルごとの絵を用意するまでは
        //! WhitePixel.pngをSkillTableEntry::panelColorで着色した四角を出しておく
        //! （絵が出来たらSkillTableEntryへアイコン用のパスを足し、そこから読むだけで差し替わる）
        Tsukino::ECS::Entity iconEntity = entt::null;

        Tsukino::ECS::Entity textEntity = entt::null;    //!< 「強欲 Lv.2」の文字
    };

    //-------------------------------------------------------------
    //! @struct PlayerSkillHudComponent
    //! @brief  画面左のHP/EXPバーの下に並べる「取得済みスキル一覧」の
    //!         エンティティ参照をまとめて持つコンポーネント。プレイヤーエンティティに付け、
    //!         PlayerSkillHudSystemが毎フレームPlayerSkillComponent::levelsへ合わせて
    //!         位置・色・文言を書き換える。
    //!         PlayerHudComponent（HP/EXPバー）と同じく固定ピクセル座標のHUDで、
    //!         WorldAnchorComponentは使わない
    //-------------------------------------------------------------
    struct PlayerSkillHudComponent {
        //! スキルの種類数ぶん用意しておく行。取得済みのものだけを上から詰めて使い、
        //! 余った行は非表示（スケール0／空文字）にする
        std::array<PlayerSkillHudRow, static_cast<size_t>(SkillId::Count)> rows{};
    };
}    // namespace CombatAndroid::ECS
