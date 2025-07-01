//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

struct ProcessLogEntry {
    std::wstring source;
    std::wstring currentTime;
    std::wstring message;
};

DWORD CreateAndMonitorProcess(std::wstring& Cmdline, std::wstring LogFormat, std::wstring ProcessCustomLogFormat);

DWORD CreateChildProcess(std::wstring& Cmdline);

static DWORD ReadFromPipe(LPVOID Param);

static size_t ClearBuffer(char* chBuf);

std::string FormatProcessLog(const std::string& line);

std::string FormatCustomLog(const std::string& line);

std::string FormatStandardLog(const std::string& line);

static size_t BufferCopy(char* dst, char* src, size_t start, size_t end);

static size_t BufferCopyAndSanitize(char* dst, char* src);

class ProcessMonitor final
{
public:
    ProcessMonitor();

    static std::wstring ProcessFieldsMapping(_In_ std::wstring eventFields, _In_ void* pLogEntryData);
};
