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
HANDLE GetLogDirHandle(std::wstring logDirectory, HANDLE stopEvent) {
    DWORD status = ERROR_SUCCESS;
    const int eventsCount = 2;

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
        const DWORD maxRetryCount = 20;
        DWORD retryCount = 1;
        LARGE_INTEGER liDueTime;
        INT64 millisecondsToWait = 15000LL;
        liDueTime.QuadPart = -millisecondsToWait*10000LL; // wait time in 100 nanoseconds

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

        HANDLE dirOpenEvents[eventsCount] = {stopEvent, timerEvent};

        while (status == ERROR_FILE_NOT_FOUND &&
                retryCount < maxRetryCount)
        {
            if (!SetWaitableTimer(timerEvent, &liDueTime, 0, NULL, NULL, 0))
            {
                status = GetLastError();
                logWriter.TraceError(
                    Utility::FormatString(
                        L"Failed to set timer object to monitor log file changes in directory %s. Error: %lu",
                        logDirectory.c_str(),
                        status
                    ).c_str()
                );
                break;
            }

            DWORD wait = WaitForMultipleObjects(eventsCount, dirOpenEvents, FALSE, INFINITE);
            switch(wait)
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
                    //Wait failed, return the failure.
                    //
                    status = GetLastError();

                    CancelWaitableTimer(timerEvent);
                    CloseHandle(timerEvent);

                    return INVALID_HANDLE_VALUE;
                }
            }

            logDirHandle = CreateFileW (logDirectory.c_str(),
                                          FILE_LIST_DIRECTORY,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          nullptr,
                                          OPEN_EXISTING,
                                          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                          nullptr);

            if (logDirHandle == INVALID_HANDLE_VALUE)
            {
                status = GetLastError();
                ++retryCount;
            }
            else
            {
                status = ERROR_SUCCESS;
                break;
            }
        }

        CancelWaitableTimer(timerEvent);
        CloseHandle(timerEvent);
    }

    return logDirHandle;
}
