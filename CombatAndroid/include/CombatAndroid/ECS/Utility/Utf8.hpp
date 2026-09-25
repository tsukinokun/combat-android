//-------------------------------------------------------------
//! @file   Utf8.hpp
//! @brief  UTF-8とワイド文字列の相互変換の宣言
//! @note   JSON（UTF-8）に書いた表示名などを、UIが使うstd::wstringへ変換するために使う
//-------------------------------------------------------------
#pragma once
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  UTF-8文字列をワイド文字列へ変換する関数
    //! @param  utf8 [in] UTF-8の文字列
    //! @return 変換結果（空文字列なら空）
    //-------------------------------------------------------------
    [[nodiscard]]
    std::wstring Utf8ToWide(const std::string& utf8);

    //-------------------------------------------------------------
    //! @brief  ワイド文字列をUTF-8文字列へ変換する関数
    //! @param  wide [in] ワイド文字列
    //! @return 変換結果（空文字列なら空）
    //-------------------------------------------------------------
    [[nodiscard]]
    std::string WideToUtf8(const std::wstring& wide);
}    // namespace CombatAndroid::ECS
