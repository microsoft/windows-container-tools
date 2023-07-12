//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

/**
 * Warapper around Create Event API
 * 
 * @param bManualReset
 * @param bInitialState
 * 
 * return event handle
*/
HANDLE CreateFileMonitorEvent(
        _In_ BOOL bManualReset,
        _In_ BOOL bInitialState
) {
    HANDLE event = CreateEvent(nullptr, bManualReset, bInitialState, nullptr);
    if(!event)
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
HANDLE GetLogDirHandle(std::wstring logDirectory, HANDLE stopEvent, std::double_t waitInSeconds) {
    DWORD status = ERROR_SUCCESS;

    HANDLE logDirHandle = CreateFileW (logDirectory.c_str(),
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
        //
        // Log directory is not created yet. Keep retrying every
        // 15 seconds for upto five minutes. Also start reading the
        // log files from the begining, instead of current end of
        // file.
        //
        HANDLE timerEvent = CreateWaitableTimer(NULL, FALSE, NULL);
        if (!timerEvent)
        {
            status = GetLastError();
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to create timer object. Log directory %ws will not be monitored for log entries. Error=%d",
                    logDirectory.c_str(),
                    status
                ).c_str()
            );
            return INVALID_HANDLE_VALUE;
        }

        logDirHandle = _RetryOpenDirectoryWithInterval(logDirectory, waitInSeconds, stopEvent, timerEvent);

        CancelWaitableTimer(timerEvent);
        CloseHandle(timerEvent);
    }

    return logDirHandle;
}

HANDLE _RetryOpenDirectoryWithInterval(std::wstring logDirectory,
                                       std::double_t waitInSeconds,
                                       HANDLE stopEvent,
                                       HANDLE timerEvent)
{
    HANDLE logDirHandle = INVALID_HANDLE_VALUE;
    DWORD status = ERROR_FILE_NOT_FOUND;
    int elapsedTime = 0;

    const int eventsCount = 2;
    HANDLE dirOpenEvents[eventsCount] = {stopEvent, timerEvent};

    while (_IsFileErrorStatus(status) && elapsedTime < waitInSeconds)
    {
        int waitInterval = _GetWaitInterval(waitInSeconds, elapsedTime);
        LARGE_INTEGER timeToWait = _ConvertWaitIntervalToLargeInt(waitInterval);

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

        logDirHandle = CreateFileW(logDirectory.c_str(),
                                   FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                   nullptr);

        if (logDirHandle == INVALID_HANDLE_VALUE)
        {
            status = GetLastError();
            elapsedTime += WAIT_INTERVAL;
        }
        else
        {
            status = ERROR_SUCCESS;
            break;
        }
    }

    return logDirHandle;
}

// Converts the time to wait to a large integer
LARGE_INTEGER _ConvertWaitIntervalToLargeInt(int timeInterval)
{
    LARGE_INTEGER liDueTime{};

    int millisecondsToWait = timeInterval * 1000;
    liDueTime.QuadPart = -millisecondsToWait * 10000LL; // wait time in 100 nanoseconds
    return liDueTime;
}

// Returns the time (in seconds) to wait based on the specified waitInSeconds
int _GetWaitInterval(std::double_t waitInSeconds, int elapsedTime)
{
    if (isinf(waitInSeconds))
    {
        return int(WAIT_INTERVAL);
    }

    if (waitInSeconds < WAIT_INTERVAL)
    {
        return int(waitInSeconds);
    }

    const auto remainingTime = int(waitInSeconds - elapsedTime);
    return remainingTime <= WAIT_INTERVAL ? remainingTime : WAIT_INTERVAL;
}

bool _IsFileErrorStatus(DWORD status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND;
}
