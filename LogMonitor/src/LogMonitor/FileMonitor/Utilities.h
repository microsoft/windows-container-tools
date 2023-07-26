//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

const int WAIT_INTERVAL = 15;

HANDLE CreateFileMonitorEvent(
    _In_ BOOL bManualReset,
    _In_ BOOL bInitialState);

HANDLE GetLogDirHandle(
    _In_ std::wstring logDirectory,
    _In_ HANDLE stopEvent,
    _In_ std::double_t waitInSeconds);

HANDLE _RetryOpenDirectoryWithInterval(
    std::wstring logDirectory,
    std::double_t waitInSeconds,
    HANDLE stopEvent,
    HANDLE timerEvent);

LARGE_INTEGER _ConvertWaitIntervalToLargeInt(
    int timeInterval);

int _GetWaitInterval(
    std::double_t waitInSeconds,
    int elapsedTime);

bool _IsFileErrorStatus(
    DWORD status);

std::wstring _GetWaitLogMessage(
    _In_ std::wstring logDirectory,
    _In_ std::double_t waitInSeconds);
