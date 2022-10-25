//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

HANDLE CreateFileMonitorEvent(
    _In_ BOOL bManualReset, 
    _In_ BOOL bInitialState);

HANDLE GetLogDirHandle(
    _In_ std::wstring logDirectory,
    _In_ HANDLE stopEvent);