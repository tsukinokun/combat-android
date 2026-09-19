//-------------------------------------------------------------
//! @file   RunResultComponent.hpp
//! @brief  RunResultComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/GameMenu.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <array>
#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum   RunOutcome
    //! @brief  1回の走行がどう終わったか
    //-------------------------------------------------------------
    enum class RunOutcome {
        None,       //!< まだ走行中
        Dead,       //!< プレイヤーが死亡した
        Cleared,    //!< 規定時間（kRunClearSeconds）を生き延びた
    };

    //! リザルトに並べる成績の行数（生存時間・撃破数・到達レベル・危険度）
    inline constexpr int kRunResultStatRowCount = 4;

    //-------------------------------------------------------------
    //! @struct RunResultStatRow
    //! @brief  成績1行ぶんのエンティティ（項目名・値・記録更新の印）
    //-------------------------------------------------------------
    struct RunResultStatRow {
        Tsukino::ECS::Entity labelEntity  = entt::null;    //!< 項目名（Font）
        Tsukino::ECS::Entity valueEntity  = entt::null;    //!< 値（Font）
        Tsukino::ECS::Entity recordEntity = entt::null;    //!< 「NEW RECORD」（Font。更新した行だけ出す）
    };

    //-------------------------------------------------------------
    //! @struct RunResultComponent
    //! @brief  走行の終わり（死亡・クリア）からリザルト表示・リトライまでの進行状態。
    //!         プレイヤーエンティティに1つだけ付ける（SkillSelectComponentと同じ考え方）
    //! @note   撃破数もここで持つ。数えるのはRunResultSystemで、走行中はHUDに出していないため
    //!         「結果として見せる値」としてここへ置いている
    //-------------------------------------------------------------
    struct RunResultComponent {
        RunOutcome outcome   = RunOutcome::None;    //!< 走行がどう終わったか
        int        killCount = 0;                   //!< 走行中に倒した敵の数

        float endElapsed      = 0.0f;     //!< 走行が終わってからの実時間（秒）。リザルトを出すまでの待ちに使う
        bool  shown           = false;    //!< リザルトを表示済みか
        bool  openedThisFrame = false;    //!< 表示した最初のフレームか（そのまま決定入力を拾わない）
        int   cursorIndex     = 0;        //!< 選択中の項目（リトライ／タイトルへ）

        Tsukino::ECS::Entity backdropEntity = entt::null;    //!< 画面全体を暗くする板（Sprite）
        Tsukino::ECS::Entity titleEntity    = entt::null;    //!< 「GAME OVER」／「CLEAR」（Font）
        Tsukino::ECS::Entity skillsEntity   = entt::null;    //!< 取ったスキルの一覧（Font）
        Tsukino::ECS::Entity bestEntity     = entt::null;    //!< ベスト記録の1行（Font）

        std::array<RunResultStatRow, kRunResultStatRowCount> statRows{};    //!< 成績の行
        GameMenuWidget                                       menu;          //!< リトライ／タイトルへ

        //-------------------------------------------------------------
        // 表示内容のスナップショット。リザルトを出した時に確定する。
        // ウィンドウサイズが変わったとき、同じ内容を新しい画面中心へ置き直すために持つ
        //-------------------------------------------------------------
        bool                                                 cleared = false;    //!< クリアで終わったか（見出しの文言と色）
        std::array<std::wstring, kRunResultStatRowCount>     statValues{};       //!< 成績の値
        std::array<bool, kRunResultStatRowCount>             statNewRecords{};   //!< 記録を更新した行（「NEW RECORD」を出す）
        std::wstring                                         skillsText;         //!< 取ったスキルの一覧。空なら出さない
        std::wstring                                         bestText;           //!< ベスト記録の1行

        float layoutWidth  = 0.0f;    //!< 最後に配置したときの画面の幅。現在の幅と違えば置き直す
        float layoutHeight = 0.0f;    //!< 同じく高さ
    };
}    // namespace CombatAndroid::ECS
