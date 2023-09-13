//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include <regex>

/**
 * Wrapper around Create Event API
 *
 * @param bManualReset
 * @param bInitialState
 *
 * return event handle
 */
HANDLE FileMonitorUtilities::CreateFileMonitorEvent(
    _In_ BOOL bManualReset,
    _In_ BOOL bInitialState)
{
    HANDLE event = CreateEvent(nullptr, bManualReset, bInitialState, nullptr);
    if (!event)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    return event;
}

/**
 * @brief Get Log Directory Handle Object
 *
 * @param logDirectory - path to get handle
 * @param stopEvent - pass an event to use when we want to stop waiting
 *
 * @return HANDLE
 */
HANDLE FileMonitorUtilities::GetLogDirHandle(
    _In_ std::wstring logDirectory,
    _In_ HANDLE stopEvent,
    _In_ std::double_t waitInSeconds)
{
    DWORD status = ERROR_SUCCESS;

    HANDLE logDirHandle = CreateFileW(logDirectory.c_str(),
                                      FILE_LIST_DIRECTORY,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr,
                                      OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                      nullptr);

    if (logDirHandle == INVALID_HANDLE_VALUE)
    {
        status = GetLastError();
    }

    if (status == ERROR_FILE_NOT_FOUND ||
        status == ERROR_PATH_NOT_FOUND)
    {
        std::wstring waitLogMesage = FileMonitorUtilities::_GetWaitLogMessage(logDirectory, waitInSeconds);
        logWriter.TraceInfo(waitLogMesage.c_str());

        //
        // Log directory is not created yet. Keep retrying every
        // 15 seconds for upto five minutes. Also start reading the
        // log files from the beginning, instead of current end of
        // file.
        //
        HANDLE timerEvent = CreateWaitableTimer(NULL, FALSE, NULL);
        if (!timerEvent)
        {
            status = GetLastError();
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to create timer object. Log directory %ws will not be monitored for log entries. Error=%d",
                    logDirectory.c_str(), status).c_str());
            return INVALID_HANDLE_VALUE;
        }

        logDirHandle = FileMonitorUtilities::_RetryOpenDirectoryWithInterval(
            logDirectory, waitInSeconds, stopEvent, timerEvent);

        CancelWaitableTimer(timerEvent);
        CloseHandle(timerEvent);
    }

    return logDirHandle;
}

void FileMonitorUtilities::ParseDirectoryValue(_Inout_ std::wstring &directory)
{
    while (!directory.empty() && directory[directory.size() - 1] == L'\\')
    {
        directory.resize(directory.size() - 1);
    }
}

bool FileMonitorUtilities::IsValidSourceFile(_In_ std::wstring directory, _In_ bool includeSubdirectories)
{
    bool isRootFolder = CheckIsRootFolder(directory);

    // The source file is invalid if the directory is a root folder and includeSubdirectories = true
    // This is because we do not monitor subfolders in the root directory
    return !(isRootFolder && includeSubdirectories);
}

bool FileMonitorUtilities::CheckIsRootFolder(_In_ std::wstring dirPath)
{
    std::wregex pattern(L"^\\w:?$");

    std::wsmatch matches;
    return std::regex_search(dirPath, matches, pattern);
}

HANDLE FileMonitorUtilities::_RetryOpenDirectoryWithInterval(
    std::wstring logDirectory,
    std::double_t waitInSeconds,
    HANDLE stopEvent,
    HANDLE timerEvent)
{
    HANDLE logDirHandle = INVALID_HANDLE_VALUE;
    DWORD status = ERROR_FILE_NOT_FOUND;
    int elapsedTime = 0;

    const int eventsCount = 2;
    HANDLE dirOpenEvents[eventsCount] = {stopEvent, timerEvent};

    while (FileMonitorUtilities::_IsFileErrorStatus(status) && elapsedTime < waitInSeconds)
    {
        int waitInterval = Utility::GetWaitInterval(waitInSeconds, elapsedTime);
        LARGE_INTEGER timeToWait = Utility::ConvertWaitIntervalToLargeInt(waitInterval);

        BOOL waitableTimer = SetWaitableTimer(timerEvent, &timeToWait, 0, NULL, NULL, 0);
        if (!waitableTimer)
        {
            status = GetLastError();
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to set timer object to monitor log file changes in directory %s. Error: %lu",
                    logDirectory.c_str(),
                    status)
                    .c_str());
            break;
        }

        DWORD wait = WaitForMultipleObjects(eventsCount, dirOpenEvents, FALSE, INFINITE);
        switch (wait)
        {
        case WAIT_OBJECT_0:
        {
            //
            // The process is exiting. Stop the timer and return.
            //
            CancelWaitableTimer(timerEvent);
            CloseHandle(timerEvent);
            return INVALID_HANDLE_VALUE;
        }

        case WAIT_OBJECT_0 + 1:
        {
            //
            // Timer event. Retry opening directory handle.
            //
            break;
        }

        default:
        {
            //
            // Wait failed, return the failure.
            //
            status = GetLastError();

            CancelWaitableTimer(timerEvent);
            CloseHandle(timerEvent);

            return INVALID_HANDLE_VALUE;
        }
        }

        logDirHandle = CreateFileW(
            logDirectory.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (logDirHandle == INVALID_HANDLE_VALUE)
        {
            status = GetLastError();
            elapsedTime += Utility::WAIT_INTERVAL;
        }
        else
        {
            logWriter.TraceInfo(
                Utility::FormatString(
                    L"Log directory %ws found after %d seconds.", logDirectory.c_str(), elapsedTime)
                    .c_str());
            status = ERROR_SUCCESS;
            break;
        }
    }

    return logDirHandle;
}

bool FileMonitorUtilities::_IsFileErrorStatus(DWORD status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND;
}

std::wstring FileMonitorUtilities::_GetWaitLogMessage(std::wstring logDirectory, std::double_t waitInSeconds)
{
    if (isinf(waitInSeconds))
    {
        return Utility::FormatString(
            L"Log directory %ws does not exist. LogMonitor will wait infinitely for the directory to be created.",
            logDirectory.c_str());
    }
    else
    {
        return Utility::FormatString(
            L"Log directory %ws does not exist. LogMonitor will wait for %.0f seconds for the directory to be created.",
            logDirectory.c_str(),
            waitInSeconds);
    }
}
