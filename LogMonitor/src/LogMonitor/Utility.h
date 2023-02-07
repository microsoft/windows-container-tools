//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

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

    static std::wstring ReplaceAll(
        _In_ std::wstring Str,
        _In_ const std::wstring& From,
        _In_ const std::wstring& To
    );

    static std::wstring STR_TO_W_STR(
        _In_ const std::string& str);

    static std::string W_STR_TO_STR(
        _In_ const std::wstring& wstr);

    static LONG GetDWORDRegKey(
        HKEY Key,
        _Inout_ const std::wstring& ValueName,
        _In_ DWORD& Value,
        _In_ DWORD DefaultValue);

    static LONG GetBoolRegKey(
        _In_ HKEY Key,
        _Inout_ const std::wstring& ValueName,
        _In_ bool& Value,
        _In_ bool DefaultValue);

    static LONG GetStringRegKey(
        _In_ HKEY Key,
        _Inout_ const std::wstring& ValueName,
        _In_ std::wstring& Value,
        _In_ const std::wstring& DefaultValue);

};
