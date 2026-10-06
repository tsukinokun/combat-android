//-------------------------------------------------------------
//! @file    UiTextSize.cpp
//! @brief   画面の文字の大きさの段階の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct UiTextSizeParams
        //! @brief  段階ごとの倍率（Assets/Tables/UiTextSize.json。ここの初期値はJSONにキーが無いときの既定値）
        //! @note   基準の文字の高さは .dfont の Size（32px）。Bodyなら 32×0.85 ≒ 27px
        //-------------------------------------------------------------
        struct UiTextSizeParams {
            float display = 3.0f;
            float title   = 2.4f;
            float heading = 1.6f;
            float large   = 1.2f;
            float body    = 0.85f;
            float smallSize = 0.65f;    // small はWindowsのヘッダが char へのマクロにしているので使えない
        };

        template <class Archive>
        void load(Archive& archive, UiTextSizeParams& params) {
            LoadField(archive, "Display", params.display);
            LoadField(archive, "Title", params.title);
            LoadField(archive, "Heading", params.heading);
            LoadField(archive, "Large", params.large);
            LoadField(archive, "Body", params.body);
            LoadField(archive, "Small", params.smallSize);
        }

        //-------------------------------------------------------------
        //! @brief  段階の倍率を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const UiTextSizeParams& GetParams() {
            static const UiTextSizeParams s_params = [] {
                UiTextSizeParams params;
                (void)LoadTableJson("UiTextSize.json", "UiTextSize", params);
                return params;
            }();
            return s_params;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 段階の倍率を得る
    //-------------------------------------------------------------
    float GetUiTextScale(UiTextSize size) {
        const UiTextSizeParams& params = GetParams();

        switch(size) {
        case UiTextSize::Display:
            return params.display;
        case UiTextSize::Title:
            return params.title;
        case UiTextSize::Heading:
            return params.heading;
        case UiTextSize::Large:
            return params.large;
        case UiTextSize::Body:
            return params.body;
        case UiTextSize::Small:
        default:
            return params.smallSize;
        }
    }
}    // namespace CombatAndroid::ECS
