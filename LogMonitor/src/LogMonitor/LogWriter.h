//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include "Utility.h"

class LogWriter final
{
public:

    LogWriter()
    {
        InitializeSRWLock(&m_stdoutLock);
    };

    ~LogWriter() {};

private:

    SRWLOCK m_stdoutLock;

public :

    bool WriteLog(
      _In_ HANDLE       FileHandle,
      _In_ LPCVOID      Buffer,
      _In_ DWORD        NumberOfBytesToWrite,
      _In_ LPDWORD      NumberOfBytesWritten,
      _In_ LPOVERLAPPED Overlapped
    )
    {
        bool result;

        AcquireSRWLockExclusive(&m_stdoutLock);
        
        result = WriteFile(FileHandle, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, Overlapped);

        ReleaseSRWLockExclusive(&m_stdoutLock);

        return result;
    }

    void WriteConsoleLog(
        _In_ const std::wstring&& LogMessage
    )
    {
        AcquireSRWLockExclusive(&m_stdoutLock);

        wprintf(L"%s\n", LogMessage.c_str());

        ReleaseSRWLockExclusive(&m_stdoutLock);
    }

    void WriteConsoleLog(
        _In_ const std::wstring& LogMessage
    )
    {
        AcquireSRWLockExclusive(&m_stdoutLock);
        
        wprintf(L"%s\n", LogMessage.c_str());

        ReleaseSRWLockExclusive(&m_stdoutLock);
    }

    void TraceError(
        _In_ LPCWSTR Message
    )
    {
        SYSTEMTIME st;
        GetSystemTime(&st);

        std::wstring formattedMessage = Utility::FormatString(L"[%s][LOGMONITOR] ERROR: %s",
            Utility::SystemTimeToString(st).c_str(),
            Message);

        WriteConsoleLog(formattedMessage);
    }

    void TraceWarning(
        _In_ LPCWSTR Message
    )
    {
        SYSTEMTIME st;
        GetSystemTime(&st);

        std::wstring formattedMessage = Utility::FormatString(L"[%s][LOGMONITOR] WARNING: %s",
            Utility::SystemTimeToString(st).c_str(),
            Message);

        WriteConsoleLog(formattedMessage);
    }

    void TraceInfo(
        _In_ LPCWSTR Message
    )
    {
        SYSTEMTIME st;
        GetSystemTime(&st);

        std::wstring formattedMessage = Utility::FormatString(L"[%s][LOGMONITOR] INFO: %s",
            Utility::SystemTimeToString(st).c_str(),
            Message);

        WriteConsoleLog(formattedMessage);
    }

};

extern LogWriter logWriter;
