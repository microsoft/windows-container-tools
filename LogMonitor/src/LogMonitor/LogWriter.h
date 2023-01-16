//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#pragma once

class LogWriter final
{
public:
    LogWriter()
    {
        InitializeSRWLock(&m_stdoutLock);

        DWORD dwMode;

        if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode))
        {
            m_isConsole = false;
        }

        m_isConsole = true;

        _setmode(_fileno(stdout), _O_U8TEXT);
    };

    ~LogWriter() {}

private:
    SRWLOCK m_stdoutLock;
    bool m_isConsole;

    void FlushStdOut()
    {
        if (m_isConsole)
        {
            fflush(stdout);
        }
    }

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

        BOOST_LOG_TRIVIAL(info) << LogMessage.data();

        FlushStdOut();

        ReleaseSRWLockExclusive(&m_stdoutLock);
    }

    void WriteConsoleLog(
        _In_ const std::wstring& LogMessage
    )
    {
        AcquireSRWLockExclusive(&m_stdoutLock);

        BOOST_LOG_TRIVIAL(info) << LogMessage.data();

        FlushStdOut();

        ReleaseSRWLockExclusive(&m_stdoutLock);
    }

    void WriteConsoleLog(
        _In_ const std::wstring& LogMessage, _In_ const std::wstring& severity
    )
    {
        AcquireSRWLockExclusive(&m_stdoutLock);

        BOOST_LOG_TRIVIAL(info) << LogMessage.data();

        FlushStdOut();

        ReleaseSRWLockExclusive(&m_stdoutLock);
    }

    void TraceError(
        _In_ LPCWSTR Message
    )
    {
        WriteConsoleLog(Message, L"error");
    }

    void TraceWarning(
        _In_ LPCWSTR Message
    )
    {
        WriteConsoleLog(Message, L"warning");
    }

    void TraceInfo(
        _In_ LPCWSTR Message
    )
    {
        WriteConsoleLog(Message, L"info");
    }
};

extern LogWriter logWriter;
