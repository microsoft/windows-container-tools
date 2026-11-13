//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"  // NOLINT(build/include_subdir)
#include <TraceLoggingProvider.h>  // NOLINT(build/include_order)
#include <fstream>  // NOLINT(build/include_order)
#include <iterator>  // NOLINT(build/include_order)
#include <string>  // NOLINT(build/include_order)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TRACELOGGING_DEFINE_PROVIDER(
    g_shutdownTestProvider,
    "LogMonitor-Shutdown-Test",
    (0xc5c68e8d, 0x3310, 0x4931, 0x9a, 0xe5, 0xdc, 0x65, 0x11, 0x60, 0x23, 0xa1));

namespace LogMonitorTests
{
TEST_CLASS(ShutdownTests)
{
    std::wstring m_directory;
    PROCESS_INFORMATION m_process{};
    HANDLE m_job = nullptr;
    HANDLE m_output = INVALID_HANDLE_VALUE;
    HANDLE m_input = INVALID_HANDLE_VALUE;
    bool m_providerRegistered = false;
    bool m_ownsTrace = false;

    static ULONG ControlSession(ULONG control)
    {
        struct
        {
            EVENT_TRACE_PROPERTIES properties;
            WCHAR name[64];
        } buffer{};
        buffer.properties.Wnode.BufferSize = sizeof(buffer);
        buffer.properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        if (control == EVENT_TRACE_CONTROL_UPDATE)
        {
            ULONG status = ControlTraceW(
                0, L"Log Monitor ETW Session", &buffer.properties, EVENT_TRACE_CONTROL_QUERY);
            if (status != ERROR_SUCCESS)
            {
                return status;
            }
            buffer.properties.FlushTimer = 60;
        }
        return ControlTraceW(0, L"Log Monitor ETW Session", &buffer.properties, control);
    }

    std::string ReadOutput()
    {
        std::ifstream output((m_directory + L"\\output.txt").c_str(), std::ios::binary);
        Assert::IsTrue(output.is_open(), L"Cannot read LogMonitor output.");
        return std::string(std::istreambuf_iterator<char>(output), std::istreambuf_iterator<char>());
    }

    void WaitForOutput(const std::string& marker)
    {
        ULONGLONG deadline = GetTickCount64() + 10000;
        while (ReadOutput().find(marker) == std::string::npos && GetTickCount64() < deadline)
        {
            Assert::AreEqual(static_cast<DWORD>(WAIT_TIMEOUT), WaitForSingleObject(m_process.hProcess, 0),
                             L"LogMonitor exited before the shutdown probe was ready.");
            Sleep(10);
        }
        Assert::IsTrue(ReadOutput().find(marker) != std::string::npos, L"Shutdown probe did not become ready.");
    }

    void CheckChildExit(DWORD expectedExitCode)
    {
        Assert::AreEqual(static_cast<ULONG>(ERROR_WMI_INSTANCE_NOT_FOUND),
                         ControlSession(EVENT_TRACE_CONTROL_QUERY),
                         L"The test requires an unused Log Monitor ETW Session and elevated execution.");

        m_directory = CreateTempDirectory();
        Assert::IsFalse(m_directory.empty(), L"Cannot create the shutdown test directory.");
        WCHAR absoluteDirectory[MAX_PATH];
        DWORD length = GetFullPathNameW(m_directory.c_str(), _countof(absoluteDirectory), absoluteDirectory, nullptr);
        Assert::IsTrue(length > 0 && length < _countof(absoluteDirectory));
        m_directory = absoluteDirectory;

        std::ofstream config((m_directory + L"\\config.json").c_str());
        config << R"({"LogConfig":{"sources":[{"type":"ETW","providers":[{)"
               << R"("providerGuid":"c5c68e8d-3310-4931-9ae5-dc65116023a1","level":"Information")"
               << R"(}]}]}})";
        config.close();
        Assert::IsTrue(config.good(), L"Cannot write the shutdown test configuration.");

        Assert::AreEqual(static_cast<TLG_STATUS>(ERROR_SUCCESS), TraceLoggingRegister(g_shutdownTestProvider));
        m_providerRegistered = true;

        HMODULE module = GetModuleHandleW(L"LogMonitorTests.dll");
        Assert::IsNotNull(module);
        WCHAR modulePath[MAX_PATH];
        length = GetModuleFileNameW(module, modulePath, _countof(modulePath));
        Assert::IsTrue(length > 0 && length < _countof(modulePath));
        std::wstring executable(modulePath);
        executable.resize(executable.find_last_of(L'\\') + 1);
        executable += L"LogMonitor.exe";

        std::wstring stopPath = Utility::ReplaceAll(m_directory + L"\\stop", L"'", L"''");
        std::wstring command = L"\"" + executable + L"\" /Config \"" + m_directory + L"\\config.json\" "
            L"powershell.exe -NoLogo -NoProfile -NonInteractive -Command "
            L"[Console]::WriteLine('CHILD_READY'); "
            L"while (!(Test-Path -LiteralPath '" + stopPath + L"')) { Start-Sleep -Milliseconds 10 }; exit " +
            std::to_wstring(expectedExitCode);

        m_job = CreateJobObjectW(nullptr, nullptr);
        Assert::IsNotNull(m_job);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        Assert::IsTrue(SetInformationJobObject(
            m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE);

        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        m_output = CreateFileW((m_directory + L"\\output.txt").c_str(), GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, &security, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        m_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Assert::IsTrue(m_output != INVALID_HANDLE_VALUE && m_input != INVALID_HANDLE_VALUE);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = m_input;
        startup.hStdOutput = m_output;
        startup.hStdError = m_output;
        // Assign the suspended monitor to the job before it can create an otherwise unowned child.
        Assert::IsTrue(CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, TRUE,
                                     CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr,
                                     &startup, &m_process) != FALSE);
        Assert::IsTrue(AssignProcessToJobObject(m_job, m_process.hProcess) != FALSE);
        m_ownsTrace = true;
        Assert::IsTrue(ResumeThread(m_process.hThread) != MAXDWORD, L"Cannot resume LogMonitor.");
        WaitForOutput("CHILD_READY");

        ULONGLONG deadline = GetTickCount64() + 10000;
        while (!TraceLoggingProviderEnabled(g_shutdownTestProvider, TRACE_LEVEL_INFORMATION, 0) &&
               GetTickCount64() < deadline)
        {
            Sleep(10);
        }
        Assert::IsTrue(TraceLoggingProviderEnabled(g_shutdownTestProvider, TRACE_LEVEL_INFORMATION, 0) != FALSE);

        // Initialize the formatter's function-local statics before process teardown.
        TraceLoggingWrite(g_shutdownTestProvider, "ShutdownProbe",
                          TraceLoggingLevel(TRACE_LEVEL_INFORMATION), TraceLoggingString("warmup", "Phase"));
        Assert::AreEqual(0UL, ControlSession(EVENT_TRACE_CONTROL_FLUSH));
        WaitForOutput("\"Phase\":\"warmup\"");
        Assert::AreEqual(0UL, ControlSession(EVENT_TRACE_CONTROL_UPDATE));

        // The extended flush timer keeps this event buffered until session stop.
        TraceLoggingWrite(g_shutdownTestProvider, "ShutdownProbe",
                          TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
                          TraceLoggingString("queued-shutdown", "Phase"));
        std::ofstream stop((m_directory + L"\\stop").c_str());
        stop.close();
        Assert::IsTrue(stop.good(), L"Cannot release the child process.");

        Assert::AreEqual(static_cast<DWORD>(WAIT_OBJECT_0), WaitForSingleObject(m_process.hProcess, 20000),
                         L"LogMonitor did not shut down after child exit.");
        DWORD exitCode = 0;
        Assert::IsTrue(GetExitCodeProcess(m_process.hProcess, &exitCode) != FALSE);
        Assert::AreEqual(expectedExitCode, exitCode, L"LogMonitor must preserve the child's exit code.");
        std::string output = ReadOutput();
        const std::wstring actualOutput = Utility::StringToWString(output);
        Assert::IsTrue(output.find("\"Phase\":\"queued-shutdown\"") != std::string::npos,
                       Utility::FormatString(L"Missing queued shutdown event. Actual output: %s",
                                             actualOutput.c_str()).c_str());
        Assert::IsTrue(output.find("[LOGMONITOR] ERROR:") == std::string::npos,
                       Utility::FormatString(L"LogMonitor reported an error. Actual output: %s",
                                             actualOutput.c_str()).c_str());
        Assert::AreEqual(static_cast<ULONG>(ERROR_WMI_INSTANCE_NOT_FOUND),
                         ControlSession(EVENT_TRACE_CONTROL_QUERY), L"LogMonitor left its ETW session running.");
        m_ownsTrace = false;
    }

 public:
    TEST_METHOD_CLEANUP(Cleanup)
    {
        bool cleaned = true;
        if (m_job != nullptr)
        {
            cleaned = TerminateJobObject(m_job, ERROR_CANCELLED) != FALSE && cleaned;
        }
        if (m_process.hProcess != nullptr)
        {
            if (WaitForSingleObject(m_process.hProcess, 5000) != WAIT_OBJECT_0)
            {
                cleaned = TerminateProcess(m_process.hProcess, ERROR_CANCELLED) != FALSE && cleaned;
                cleaned = WaitForSingleObject(m_process.hProcess, 5000) == WAIT_OBJECT_0 && cleaned;
            }
        }
        for (HANDLE handle : {m_process.hThread, m_process.hProcess, m_job, m_output, m_input})
        {
            if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
            {
                cleaned = CloseHandle(handle) != FALSE && cleaned;
            }
        }
        if (m_providerRegistered)
        {
            TraceLoggingUnregister(g_shutdownTestProvider);
        }
        if (m_ownsTrace)
        {
            ULONG status = ControlSession(EVENT_TRACE_CONTROL_STOP);
            cleaned = (status == ERROR_SUCCESS || status == ERROR_WMI_INSTANCE_NOT_FOUND) && cleaned;
        }
        if (!m_directory.empty())
        {
            for (const auto* name : {L"config.json", L"output.txt", L"stop"})
            {
                if (!DeleteFileW((m_directory + L"\\" + name).c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
                {
                    cleaned = false;
                }
            }
            cleaned = RemoveDirectoryW(m_directory.c_str()) != FALSE && cleaned;
        }
        Assert::IsTrue(cleaned, L"Failed to clean up shutdown test resources.");
    }

    TEST_METHOD(TestNormalChildExitWithPendingEtwEvents)
    {
        CheckChildExit(0);
    }

    TEST_METHOD(TestNonzeroChildExitWithPendingEtwEvents)
    {
        CheckChildExit(37);
    }
};
}  // namespace LogMonitorTests
