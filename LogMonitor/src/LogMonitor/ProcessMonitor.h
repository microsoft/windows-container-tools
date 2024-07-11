//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

struct ProcessLogEntry {
    std::wstring source;
    std::wstring currentTime;
    std::wstring logLine;
};

DWORD CreateAndMonitorProcess(std::wstring& Cmdline, std::wstring LogFormat, std::wstring ProcessCustomLogFormat);

DWORD CreateChildProcess(std::wstring& Cmdline);

static DWORD ReadFromPipe(LPVOID Param);

static size_t clearBuffer(char* chBuf);

size_t formatProcessLog(char* chBuf);

size_t formatCustomLog(char* chBuf);

size_t formatStandardLog(char* chBuf);

static size_t bufferCopy(char* dst, char* src, size_t start, size_t end);

static size_t bufferCopyAndSanitize(char* dst, char* src);

class ProcessMonitor final
{
public:
    ProcessMonitor();

    static std::wstring ProcessFieldsMapping(_In_ std::wstring eventFields, _In_ void* pLogEntryData);

};