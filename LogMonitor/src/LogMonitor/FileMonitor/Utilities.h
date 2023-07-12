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
    _In_ std::wstring logDirectory,
    _In_ std::double_t waitInSeconds,
    _In_ HANDLE stopEvent,
    _In_ HANDLE timerEvent);


LARGE_INTEGER _ConvertWaitIntervalToLargeInt(
    _In_ int timeInterval);

int _GetWaitInterval(
    _In_ std::double_t waitInSeconds,
    _In_ int elapsedTime);

bool _IsFileErrorStatus(
    _In_ DWORD status);
