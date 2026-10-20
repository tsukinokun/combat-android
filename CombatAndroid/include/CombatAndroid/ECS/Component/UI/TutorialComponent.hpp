//-------------------------------------------------------------
//! @file   TutorialComponent.hpp
//! @brief  TutorialComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/UI/InputPromptWidget.hpp>

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
        Goal,           //!< 目標（群れを生き延び、夜の主を討て）を出して終わる
        Count,
    };

    //-------------------------------------------------------------
    //! @enum   TutorialHint
    //! @brief  手順の流れとは別に、その場面が来たときに出す案内（番号は付けない）
    //! @note   開始時は武器が落ちていないので、拾う・切り替える・溜めるは手に入ったときに教える。
    //!         同時に条件がそろったらこの並びで1つずつ出す
    //-------------------------------------------------------------
    enum class TutorialHint : int {
        Pickup = 0,    //!< 拾える武器が初めて地面に現れた → 触れて拾う
        Switch,        //!< 手持ちが2本になった → ホイールで切り替える
        Charge,        //!< バトルアックスを持った → 長押しで溜め攻撃
        Count,
    };

    //-------------------------------------------------------------
    //! @enum   TutorialHintState
    //! @brief  場面の案内1つの状態
    //-------------------------------------------------------------
    enum class TutorialHintState : int {
        Waiting = 0,    //!< まだ場面が来ていない
        Pending,        //!< 場面が来た（ほかの案内が出ていれば、それが消えるのを待つ）
        Showing,        //!< 出している（できるまで出し続ける）
        Done,           //!< できた（以後は出さない）
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

        Tsukino::ECS::Entity panelEntity   = entt::null;    //!< 画面下の半透明の板（Sprite）
        Tsukino::ECS::Entity textEntity    = entt::null;    //!< 案内の文（Font）
        Tsukino::ECS::Entity counterEntity = entt::null;    //!< 「操作 2 / 3」（Font）

        //! 手順ごとのキー表示（Goalは使わない）
        std::array<InputPromptWidget, static_cast<size_t>(TutorialStep::Count)> prompts{};

        //-------------------------------------------------------------
        // 場面の案内（拾う・切り替える・溜める）。手順の流れには入れず、場面が来た時点で番号無しで出す。
        // 一度に出すのは1つだけなので、板と文は共用し、キー表示だけ案内ごとに持つ。
        // 手順の案内が出ている間はその上に積む
        //-------------------------------------------------------------
        std::array<TutorialHintState, static_cast<size_t>(TutorialHint::Count)> hintStates{};
        std::array<InputPromptWidget, static_cast<size_t>(TutorialHint::Count)> hintPrompts{};    //!< キー表示（拾うは無し）
        int                  activeHint    = -1;            //!< 出している案内（TutorialHintの番号。-1なら無し）
        float                hintDoneTimer = -1.0f;         //!< できてからの経過秒数（0以上の間は「OK」を出して消えるのを待つ）
        Tsukino::ECS::Entity hintPanel     = entt::null;    //!< 案内の板（Sprite）
        Tsukino::ECS::Entity hintText      = entt::null;    //!< 案内の文（Font）

        //! 案内を出した瞬間の手持ち。拾った・切り替えたの判定に「出した時点からの変化」を使う
        int hintInventorySize   = 0;
        int hintWeaponLevelSum  = 0;    //!< 手持ちの武器レベルの合計（同じ種類を拾うとレベルが上がるだけなので、それも拾ったとみなす）
        int hintSelectedWeapon  = 0;

        //-------------------------------------------------------------
        // 動き（UiMotion）のための表示状態。隠したら倒し、次に出すときにまた滑り込ませる
        //-------------------------------------------------------------
        int   shownStep        = -1;       //!< 板を出してある手順（変わったら板を下から滑り込ませる。-1なら隠れている）
        bool  stepsFadingOut   = false;    //!< 目標を出し終えて、手順の板が消えていく途中か
        bool  hintShown        = false;    //!< 場面の案内を出してあるか
        bool  hintFading       = false;    //!< 場面の案内が消えていく途中か
        float hintLastY        = 0.0f;     //!< 場面の案内を前回置いたY（積む位置が変わったら滑らせて移す）
    };
}    // namespace CombatAndroid::ECS
