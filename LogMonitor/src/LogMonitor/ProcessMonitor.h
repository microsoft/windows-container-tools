//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

DWORD CreateAndMonitorProcess(std::wstring& Cmdline, LoggerSettings& Config);
std::wstring m_logFormat;

class ProcessMonitor final
{
public:
    ProcessMonitor() = delete;

    ProcessMonitor(
        _In_ const std::wstring& customLogFormat
    );

    ~ProcessMonitor();

};