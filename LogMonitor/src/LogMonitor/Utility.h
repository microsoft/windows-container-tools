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

    static bool isJsonNumber(_In_ std::wstring& str);

    static void SanitizeJson(_Inout_ std::wstring& str);

    static bool CompareWStrings(
        _In_ std::wstring stringA,
        _In_ std::wstring stringB
    );

    static std::wstring FormatEventLineLog(_In_ std::wstring customLogFormat, _In_ void* pLogEntry, _In_ std::wstring sourceType);

    static bool isCustomJsonFormat(_Inout_ std::wstring& customLogFormat);
};
