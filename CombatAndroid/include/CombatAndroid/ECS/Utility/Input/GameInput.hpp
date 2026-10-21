//-------------------------------------------------------------
//! @file    GameInput.hpp
//! @brief   ゲームの操作（攻撃・回避・決定など）を、キー・マウス・ゲームパッドの区別なく読む窓口の宣言
//! @note    割り当ては Assets/Tables/InputBindings.json（ルートキー InputBindings）。
//!          ゲームのコードはキーやボタンを直接見ず、必ずここの関数で読む。
//!          デバッグ用のキー（F1〜F12 など）だけは例外で、各 System がキーを直接見てよい
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/Input/InputActionMap.hpp>
#include <Tsukino/Core/typedef.hpp>

#include <hlsl++.h>

#include <string>
#include <string_view>

// 前方宣言
namespace Tsukino::Input {
    class InputSystem;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum   GameAction
    //! @brief  ゲームの操作の種類
    //! @note   並びは GameInput.cpp の kGameActionKeys（JSON 上の名前）と揃える
    //-------------------------------------------------------------
    enum class GameAction : Tsukino::u16 {
        Attack = 0,    //!< 攻撃（長押しで溜め）
        Dodge,         //!< 回避
        Sprint,        //!< ダッシュ（押しているあいだだけ。Shift・LT）
        SprintToggle,  //!< ダッシュの切り替え（押すとダッシュが続き、止まると解除。L3）
        WeaponNext,    //!< 次の武器へ持ち替える
        WeaponPrev,    //!< 前の武器へ持ち替える
        Pause,         //!< ポーズを開く
        Confirm,       //!< 決定
        Cancel,        //!< 戻る・閉じる
        MenuUp,        //!< メニューの上の項目へ
        MenuDown,      //!< メニューの下の項目へ
        MenuLeft,      //!< メニューの値を減らす・左のカードへ
        MenuRight,     //!< メニューの値を増やす・右のカードへ
        MoveForward,   //!< 前へ移動（キーの分。スティックは moveStick）
        MoveBack,      //!< 後ろへ移動
        MoveLeft,      //!< 左へ移動
        MoveRight,     //!< 右へ移動

        Count
    };

    //-------------------------------------------------------------
    //! @brief  操作が入力された瞬間か
    //! @param  input  [in] 入力システム
    //! @param  action [in] 操作
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsActionPressed(const Tsukino::Input::InputSystem& input, GameAction action);

    //-------------------------------------------------------------
    //! @brief  操作が入力されているか（押しっぱなし）
    //! @param  input  [in] 入力システム
    //! @param  action [in] 操作
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsActionDown(const Tsukino::Input::InputSystem& input, GameAction action);

    //-------------------------------------------------------------
    //! @brief  操作が離された瞬間か
    //! @param  input  [in] 入力システム
    //! @param  action [in] 操作
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsActionReleased(const Tsukino::Input::InputSystem& input, GameAction action);

    //-------------------------------------------------------------
    //! @brief  移動の入力を読む
    //! @param  input [in] 入力システム
    //! @return x が右、y が前。キーは長さ1、スティックはアナログ値のまま（長さ0〜1）
    //-------------------------------------------------------------
    [[nodiscard]]
    hlslpp::float2 ReadMoveAxis(const Tsukino::Input::InputSystem& input);

    //-------------------------------------------------------------
    //! @brief  カメラを回すスティックの入力を読む
    //! @param  input [in] 入力システム
    //! @return x が右、y が上。長さ0〜1
    //-------------------------------------------------------------
    [[nodiscard]]
    hlslpp::float2 ReadCameraStick(const Tsukino::Input::InputSystem& input);

    //-------------------------------------------------------------
    //! @brief  スティックを倒しきったときのカメラの回る速さ（rad/秒）
    //! @return 左右（yaw）の速さ。上下は GetCameraStickPitchSpeed
    //-------------------------------------------------------------
    [[nodiscard]]
    float GetCameraStickYawSpeed();

    //-------------------------------------------------------------
    //! @brief  スティックを倒しきったときのカメラの上下に回る速さ（rad/秒）
    //-------------------------------------------------------------
    [[nodiscard]]
    float GetCameraStickPitchSpeed();

    //-------------------------------------------------------------
    //! @brief  最後に触った入力機器がゲームパッドか
    //! @param  input [in] 入力システム
    //! @note   操作表示（キーキャップ・案内の文字）の切り替えに使う
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsUsingGamepad(const Tsukino::Input::InputSystem& input);

    //-------------------------------------------------------------
    //! @brief  操作の割り当てを得る
    //! @param  action [in] 操作
    //-------------------------------------------------------------
    [[nodiscard]]
    const Tsukino::Input::InputBinding& GetActionBinding(GameAction action);

    //-------------------------------------------------------------
    //! @brief  操作を画面に出すときの表記を得る（最後に触った機器に合わせる）
    //! @param  input  [in] 入力システム
    //! @param  action [in] 操作
    //! @return "SPACE" "左クリック" "A" "LB" など。割り当てが無ければ空文字
    //-------------------------------------------------------------
    [[nodiscard]]
    std::wstring GetActionLabel(const Tsukino::Input::InputSystem& input, GameAction action);

    //-------------------------------------------------------------
    //! @brief  操作の割り当てがマウスの左ボタンか（キーボード・マウスで遊んでいるとき）
    //! @param  input  [in] 入力システム
    //! @param  action [in] 操作
    //! @note   操作表示でキーキャップの代わりにマウスの絵を出すかの判定に使う
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsActionShownAsMouse(const Tsukino::Input::InputSystem& input, GameAction action);

    //-------------------------------------------------------------
    //! @brief  案内の文の中の {操作名} を、最後に触った機器の表記へ置き換える
    //! @param  input    [in] 入力システム
    //! @param  text [in] 置き換える前の文（例: L"{Dodge} で回避"）
    //! @return 置き換えた文（キーボード: L"SPACE で回避"、パッド: L"A で回避"）
    //! @note   {操作名} は GameAction の名前（{Attack} {Dodge} など）のほか、
    //!         {Move}（移動）{Dash}（ダッシュ。パッドは切り替えと長押しを並べる）{Camera}（カメラ）{WeaponSwitch}（武器の切り替え）が使える。知らない名前はそのまま残す
    //-------------------------------------------------------------
    [[nodiscard]]
    std::wstring FormatInputText(const Tsukino::Input::InputSystem& input, std::wstring_view text);
}    // namespace CombatAndroid::ECS
