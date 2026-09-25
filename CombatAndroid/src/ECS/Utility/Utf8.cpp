//-------------------------------------------------------------
//! @file   Utf8.cpp
//! @brief  UTF-8とワイド文字列の相互変換の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Utf8.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief UTF-8文字列をワイド文字列へ変換する
    //-------------------------------------------------------------
    std::wstring Utf8ToWide(const std::string& utf8) {
        if(utf8.empty())
            return {};

        const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        if(length <= 0)
            return {};

        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
        return wide;
    }

    //-------------------------------------------------------------
    //! @brief ワイド文字列をUTF-8文字列へ変換する
    //-------------------------------------------------------------
    std::string WideToUtf8(const std::wstring& wide) {
        if(wide.empty())
            return {};

        const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if(length <= 0)
            return {};

        std::string utf8(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), length, nullptr, nullptr);
        return utf8;
    }
}    // namespace CombatAndroid::ECS
