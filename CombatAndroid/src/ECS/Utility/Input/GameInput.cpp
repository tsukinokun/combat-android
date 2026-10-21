//-------------------------------------------------------------
//! @file    GameInput.cpp
//! @brief   ゲームの操作を、キー・マウス・ゲームパッドの区別なく読む窓口の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Input/GameInput.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/Core/Input/InputNames.hpp>
#include <Tsukino/Core/Input/InputSystem.hpp>

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <array>
#include <iterator>
#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr const char* kInputTableFile = "InputBindings.json";
        constexpr const char* kInputTableRoot = "InputBindings";

        //! JSON上の操作名（GameActionの並び順）
        constexpr const char* kGameActionKeys[] = {
            "Attack", "Dodge", "Sprint", "SprintToggle", "WeaponNext", "WeaponPrev", "Pause", "Confirm", "Cancel",
            "MenuUp", "MenuDown", "MenuLeft", "MenuRight", "MoveForward", "MoveBack", "MoveLeft", "MoveRight",
        };

        // 操作を足したのに名前を書き忘れる事故を防ぐ
        static_assert(std::size(kGameActionKeys) == static_cast<size_t>(GameAction::Count),
                      "GameAction に操作を足したら kGameActionKeys にも1つ足すこと");

        //-------------------------------------------------------------
        //! @struct BindingEntry
        //! @brief  JSON上の割り当て1つぶん（キーとボタンは名前のまま持つ）
        //-------------------------------------------------------------
        struct BindingEntry {
            std::vector<std::string> keys;         //!< キー名（KeyCode の綴り）
            std::vector<std::string> buttons;      //!< ボタン名（GamepadButton の綴り）
            int                      wheel = 0;    //!< ホイールの向き（+1 奥 / −1 手前 / 0 使わない）
        };

        template <class Archive>
        void save(Archive& archive, const BindingEntry& entry) {
            archive(cereal::make_nvp("keys", entry.keys), cereal::make_nvp("buttons", entry.buttons), cereal::make_nvp("wheel", entry.wheel));
        }

        template <class Archive>
        void load(Archive& archive, BindingEntry& entry) {
            LoadField(archive, "keys", entry.keys);
            LoadField(archive, "buttons", entry.buttons);
            LoadField(archive, "wheel", entry.wheel);
        }

        //-------------------------------------------------------------
        //! @struct InputTableData
        //! @brief  割り当ての表全体
        //! @note   既定値は JSON が無くても今までどおり遊べる割り当て（JSON と同じ中身）
        //-------------------------------------------------------------
        struct InputTableData {
            std::array<BindingEntry, static_cast<size_t>(GameAction::Count)> entries = { {
                { { "LButton" }, { "X" }, 0 },                                // Attack
                { { "Space" }, { "A" }, 0 },                                  // Dodge
                { { "Shift" }, { "LeftTrigger" }, 0 },                        // Sprint
                { {}, { "LeftThumb" }, 0 },                                   // SprintToggle
                { {}, { "RightShoulder" }, 1 },                               // WeaponNext
                { {}, { "LeftShoulder" }, -1 },                               // WeaponPrev
                { { "Escape" }, { "Start" }, 0 },                             // Pause
                { { "F", "Enter" }, { "A" }, 0 },                             // Confirm
                { { "Escape" }, { "B" }, 0 },                                 // Cancel
                { { "W", "Up" }, { "DPadUp", "LeftStickUp" }, 1 },            // MenuUp
                { { "S", "Down" }, { "DPadDown", "LeftStickDown" }, -1 },     // MenuDown
                { { "A", "Left" }, { "DPadLeft", "LeftStickLeft" }, 0 },      // MenuLeft
                { { "D", "Right" }, { "DPadRight", "LeftStickRight" }, 0 },   // MenuRight
                { { "W", "Up" }, {}, 0 },                                     // MoveForward
                { { "S", "Down" }, {}, 0 },                                   // MoveBack
                { { "A", "Left" }, {}, 0 },                                   // MoveLeft
                { { "D", "Right" }, {}, 0 },                                  // MoveRight
            } };

            float cameraStickYawSpeed   = 3.2f;    //!< 右スティックを倒しきったときの左右の回る速さ（rad/秒）
            float cameraStickPitchSpeed = 2.0f;    //!< 同じく上下の回る速さ（rad/秒）

            //! 名前から引き直した割り当て（読み込みのたびに作り直す）
            Tsukino::Input::InputActionMap actionMap;
        };

        //-------------------------------------------------------------
        //! @brief  JSON上の割り当てを、エンジンの割り当てに変換する関数
        //! @param  entry      [in] JSON上の割り当て
        //! @param  actionName [in] 操作名（ログ用）
        //! @return 変換した割り当て。知らない名前は読み飛ばしてログに出す
        //-------------------------------------------------------------
        Tsukino::Input::InputBinding ToBinding(const BindingEntry& entry, const char* actionName) {
            Tsukino::Input::InputBinding binding;
            for(const std::string& name : entry.keys) {
                const Tsukino::Input::KeyCode code = Tsukino::Input::KeyCodeFromName(name);
                if(code == Tsukino::Input::KeyCode::None) {
                    Tsukino::Core::Log::Error(std::string("InputBindings.json: unknown key '") + name + "' in " + actionName);
                    continue;
                }
                binding.keys.push_back(code);
            }
            for(const std::string& name : entry.buttons) {
                const Tsukino::Input::GamepadButton button = Tsukino::Input::GamepadButtonFromName(name);
                if(button == Tsukino::Input::GamepadButton::Count) {
                    Tsukino::Core::Log::Error(std::string("InputBindings.json: unknown button '") + name + "' in " + actionName);
                    continue;
                }
                binding.buttons.push_back(button);
            }
            binding.wheelDirection = entry.wheel;
            return binding;
        }

        template <class Archive>
        void save(Archive& archive, const InputTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i)
                archive(cereal::make_nvp(kGameActionKeys[i], data.entries[i]));
            archive(cereal::make_nvp("cameraStickYawSpeed", data.cameraStickYawSpeed),
                    cereal::make_nvp("cameraStickPitchSpeed", data.cameraStickPitchSpeed));
        }

        template <class Archive>
        void load(Archive& archive, InputTableData& data) {
            for(size_t i = 0; i < data.entries.size(); ++i)
                LoadField(archive, kGameActionKeys[i], data.entries[i]);
            LoadField(archive, "cameraStickYawSpeed", data.cameraStickYawSpeed);
            LoadField(archive, "cameraStickPitchSpeed", data.cameraStickPitchSpeed);
        }

        //-------------------------------------------------------------
        //! @brief  名前の割り当てから、エンジンの割り当てを作り直す関数
        //! @param  data [in,out] 割り当ての表
        //-------------------------------------------------------------
        void RebuildActionMap(InputTableData& data) {
            data.actionMap = Tsukino::Input::InputActionMap{};
            for(size_t i = 0; i < data.entries.size(); ++i)
                data.actionMap.SetBinding(static_cast<Tsukino::u16>(i), ToBinding(data.entries[i], kGameActionKeys[i]));
        }

        //-------------------------------------------------------------
        //! @brief  割り当ての表を Assets/Tables/InputBindings.json から読む関数
        //! @param  data [out] 割り当ての表
        //! @return 読めたか（読めなくても既定値の割り当ては作る）
        //-------------------------------------------------------------
        bool LoadInputTable(InputTableData& data) {
            const bool loaded = LoadTableJson(kInputTableFile, kInputTableRoot, data);
            RebuildActionMap(data);
            return loaded;
        }

        //-------------------------------------------------------------
        //! @brief  割り当ての表を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const InputTableData& GetInputTable() {
            static Tsukino::IO::HotReloadable<InputTableData> s_table(TablePath(kInputTableFile), LoadInputTable);
            return s_table.Get();
        }

        //-------------------------------------------------------------
        //! @brief  操作の番号（エンジンの割り当ての添字）
        //-------------------------------------------------------------
        Tsukino::u16 ToId(GameAction action) {
            return static_cast<Tsukino::u16>(action);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 操作が入力された瞬間か
    //-------------------------------------------------------------
    bool IsActionPressed(const Tsukino::Input::InputSystem& input, GameAction action) {
        return GetInputTable().actionMap.IsPressed(input, ToId(action));
    }

    //-------------------------------------------------------------
    //! @brief 操作が入力されているか（押しっぱなし）
    //-------------------------------------------------------------
    bool IsActionDown(const Tsukino::Input::InputSystem& input, GameAction action) {
        return GetInputTable().actionMap.IsDown(input, ToId(action));
    }

    //-------------------------------------------------------------
    //! @brief 操作が離された瞬間か
    //-------------------------------------------------------------
    bool IsActionReleased(const Tsukino::Input::InputSystem& input, GameAction action) {
        return GetInputTable().actionMap.IsReleased(input, ToId(action));
    }

    //-------------------------------------------------------------
    //! @brief 移動の入力を読む
    //-------------------------------------------------------------
    hlslpp::float2 ReadMoveAxis(const Tsukino::Input::InputSystem& input) {
        const Tsukino::Input::InputActionMap& map = GetInputTable().actionMap;

        Tsukino::Input::InputAxisBinding axis;
        axis.up    = map.GetBinding(ToId(GameAction::MoveForward));
        axis.down  = map.GetBinding(ToId(GameAction::MoveBack));
        axis.left  = map.GetBinding(ToId(GameAction::MoveLeft));
        axis.right = map.GetBinding(ToId(GameAction::MoveRight));
        axis.stick = Tsukino::Input::GamepadStick::Left;
        return Tsukino::Input::ReadAxis(input, axis);
    }

    //-------------------------------------------------------------
    //! @brief カメラを回すスティックの入力を読む
    //-------------------------------------------------------------
    hlslpp::float2 ReadCameraStick(const Tsukino::Input::InputSystem& input) {
        return input.GetRightStick();
    }

    //-------------------------------------------------------------
    //! @brief スティックを倒しきったときのカメラの左右に回る速さ
    //-------------------------------------------------------------
    float GetCameraStickYawSpeed() {
        return GetInputTable().cameraStickYawSpeed;
    }

    //-------------------------------------------------------------
    //! @brief スティックを倒しきったときのカメラの上下に回る速さ
    //-------------------------------------------------------------
    float GetCameraStickPitchSpeed() {
        return GetInputTable().cameraStickPitchSpeed;
    }

    //-------------------------------------------------------------
    //! @brief 最後に触った入力機器がゲームパッドか
    //-------------------------------------------------------------
    bool IsUsingGamepad(const Tsukino::Input::InputSystem& input) {
        return input.GetLastInputDevice() == Tsukino::Input::InputDevice::Gamepad;
    }

    //-------------------------------------------------------------
    //! @brief 操作の割り当てを得る
    //-------------------------------------------------------------
    const Tsukino::Input::InputBinding& GetActionBinding(GameAction action) {
        return GetInputTable().actionMap.GetBinding(ToId(action));
    }

    //-------------------------------------------------------------
    //! @brief 操作を画面に出すときの表記を得る
    //-------------------------------------------------------------
    std::wstring GetActionLabel(const Tsukino::Input::InputSystem& input, GameAction action) {
        return Tsukino::Input::GetBindingDisplayName(GetActionBinding(action), input.GetLastInputDevice());
    }

    //-------------------------------------------------------------
    //! @brief 案内の文の中の {操作名} を、最後に触った機器の表記へ置き換える
    //-------------------------------------------------------------
    std::wstring FormatInputText(const Tsukino::Input::InputSystem& input, std::wstring_view text) {
        const bool gamepad = IsUsingGamepad(input);

        //-------------------------------------------------------------
        // 操作1つでは表せない、まとめた呼び名
        //-------------------------------------------------------------
        auto resolveSpecial = [&](std::wstring_view name, std::wstring& out) {
            if(name == L"Move") {
                if(gamepad) {
                    out = L"左スティック";
                } else {
                    out = GetActionLabel(input, GameAction::MoveForward) + L" " + GetActionLabel(input, GameAction::MoveLeft) + L" " +
                          GetActionLabel(input, GameAction::MoveBack) + L" " + GetActionLabel(input, GameAction::MoveRight);
                }
                return true;
            }
            if(name == L"Dash") {
                // パッドは切り替え（L3）と押しているあいだ（LT）の2つがあるので並べて出す
                const std::wstring hold   = GetActionLabel(input, GameAction::Sprint);
                const std::wstring toggle = GetActionLabel(input, GameAction::SprintToggle);
                if(toggle.empty())
                    out = hold;
                else if(hold.empty())
                    out = toggle;
                else
                    out = toggle + L" / " + hold;
                return true;
            }
            if(name == L"Camera") {
                out = gamepad ? L"右スティック" : L"マウス";
                return true;
            }
            if(name == L"WeaponSwitch") {
                if(gamepad)
                    out = GetActionLabel(input, GameAction::WeaponPrev) + L" / " + GetActionLabel(input, GameAction::WeaponNext);
                else
                    out = GetActionBinding(GameAction::WeaponNext).wheelDirection != 0 ? L"マウスホイール" : GetActionLabel(input, GameAction::WeaponNext);
                return true;
            }
            return false;
        };

        std::wstring result;
        result.reserve(text.size());

        size_t cursor = 0;
        while(cursor < text.size()) {
            const size_t open = text.find(L'{', cursor);
            if(open == std::wstring_view::npos) {
                result.append(text.substr(cursor));
                break;
            }
            const size_t close = text.find(L'}', open);
            if(close == std::wstring_view::npos) {
                result.append(text.substr(cursor));
                break;
            }

            result.append(text.substr(cursor, open - cursor));
            const std::wstring_view name = text.substr(open + 1, close - open - 1);

            std::wstring label;
            bool         resolved = resolveSpecial(name, label);
            for(size_t i = 0; !resolved && i < std::size(kGameActionKeys); ++i) {
                // 操作名は ASCII だけなので、1文字ずつ比べる
                const std::string_view key = kGameActionKeys[i];
                if(key.size() != name.size())
                    continue;
                bool same = true;
                for(size_t c = 0; c < key.size() && same; ++c)
                    same = static_cast<wchar_t>(key[c]) == name[c];
                if(same) {
                    label    = GetActionLabel(input, static_cast<GameAction>(i));
                    resolved = true;
                }
            }

            if(resolved)
                result.append(label);
            else
                result.append(text.substr(open, close - open + 1));    // 知らない名前はそのまま残して気づけるようにする
            cursor = close + 1;
        }
        return result;
    }

    //-------------------------------------------------------------
    //! @brief 操作の割り当てがマウスの左ボタンか
    //-------------------------------------------------------------
    bool IsActionShownAsMouse(const Tsukino::Input::InputSystem& input, GameAction action) {
        if(IsUsingGamepad(input))
            return false;
        const Tsukino::Input::InputBinding& binding = GetActionBinding(action);
        return !binding.keys.empty() && binding.keys.front() == Tsukino::Input::KeyCode::LButton;
    }
}    // namespace CombatAndroid::ECS
