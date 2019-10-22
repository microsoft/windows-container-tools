//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#include <Windows.h>
#include <functional>
#include <string>

class Utility final
{
public:

    static std::wstring SystemTimeToString(
        SYSTEMTIME SystemTime
    );

    static std::wstring FileTimeToString(
        FILETIME FileTime
    );

    static std::wstring FormatString(
        _In_ _Printf_format_string_ LPCWSTR FormatString,
        ...
    );

    static bool IsTextUTF8(
        _In_ LPCSTR InputStream,
        _In_ int Length
    );

    static bool IsInputTextUnicode(
        _In_ LPCSTR InputStream,
        _In_ int Length
    );

    static std::wstring GetShortPath(
        _In_ const std::wstring& Path
    );

    static std::wstring GetLongPath(
        _In_ const std::wstring& Path
    );

};
