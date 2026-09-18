//-------------------------------------------------------------
//! @file   TutorialComponent.hpp
//! @brief  TutorialComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/InputPromptWidget.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <entt/entt.hpp>

#include <array>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum   TutorialStep
    //! @brief  操作の案内の手順（並び順どおりに進む）
    //-------------------------------------------------------------
    enum class TutorialStep : int {
        Move = 0,       //!< WASDで移動
        Attack,         //!< 左クリックで攻撃
        Dodge,          //!< Spaceで回避
        Pickup,         //!< Fで武器を拾う
        Switch,         //!< ホイールで武器を切り替える（2本以上持っていないときは飛ばす）
        Charge,         //!< バトルアックスで溜め攻撃（持っていないときは飛ばす）
        Goal,           //!< 目標（10分生き延びろ）を出して終わる
        Count,
    };

    //-------------------------------------------------------------
    //! @struct TutorialComponent
    //! @brief  タイトルから始めたときにだけ出す、操作の案内の状態とUIエンティティ
    //! @note   CombatAndroidSceneがタイトル経由で作られたときだけ、専用のエンティティに1つ付ける。
    //!         リトライで始めたときは付けないので、TutorialSystemは何もしない
    //-------------------------------------------------------------
    struct TutorialComponent {
        TutorialStep step      = TutorialStep::Move;    //!< 今の手順
        float        stepTimer = 0.0f;                  //!< 今の手順を出してからの経過秒数
        float        doneTimer = -1.0f;                 //!< 手順をこなしてからの経過秒数（0以上の間は「OK」を出して次へ進むのを待つ）
        float        moveTime  = 0.0f;                  //!< 移動の手順で、実際に歩いた合計秒数
        bool         finished  = false;                 //!< 全部終わったか（以後は何も出さない）

        //! 手順を出した瞬間の手持ち。拾った・切り替えたの判定に「始めた時点からの変化」を使う
        int inventorySizeAtStep     = 0;
        int selectedWeaponAtStep    = 0;

        Tsukino::ECS::Entity panelEntity   = entt::null;    //!< 画面下の半透明の板（Sprite）
        Tsukino::ECS::Entity textEntity    = entt::null;    //!< 案内の文（Font）
        Tsukino::ECS::Entity counterEntity = entt::null;    //!< 「操作 2 / 6」（Font）

        //! 手順ごとのキー表示（Goalは使わない）
        std::array<InputPromptWidget, static_cast<size_t>(TutorialStep::Count)> prompts{};
    };
}    // namespace CombatAndroid::ECS
