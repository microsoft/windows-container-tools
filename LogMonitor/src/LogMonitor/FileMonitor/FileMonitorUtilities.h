//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

class FileMonitorUtilities final
{
    public:

        static HANDLE CreateFileMonitorEvent(
            _In_ BOOL bManualReset,
            _In_ BOOL bInitialState);

        static HANDLE GetLogDirHandle(
            _In_ std::wstring logDirectory,
            _In_ HANDLE stopEvent,
            _In_ std::double_t waitInSeconds);

        static void ParseDirectoryValue(_Inout_ std::wstring &directory);

        static bool IsValidSourceFile(_In_ std::wstring directory, bool includeSubdirectories);

        static bool CheckIsRootFolder(_In_ std::wstring dirPath);

    private:
        static HANDLE _RetryOpenDirectoryWithInterval(
            std::wstring logDirectory,
            std::double_t waitInSeconds,
            HANDLE stopEvent,
            HANDLE timerEvent);

        static bool _IsFileErrorStatus(DWORD status);

        static std::wstring _GetWaitLogMessage(
            std::wstring logDirectory,
            std::double_t waitInSeconds);

        static std::wstring _GetParentDir(
            std::wstring dirPath);
};
