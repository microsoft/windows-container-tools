//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"  // NOLINT(build/include_subdir)
#include <cstdio>  // NOLINT(build/include_order)
#include <memory>  // NOLINT(build/include_order)
#include <vector>  // NOLINT(build/include_order)


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#define BUFFER_SIZE 65536

namespace LogMonitorTests
{
///
/// Tests of Event Monitor. This class writes events and tests that the
/// Monitor prints them to stdout.
///
TEST_CLASS(EventMonitorTests)
{
    ///
    /// The waiting time after event creation
    ///
    const DWORD WAIT_TIME_EVENTMONITOR_START = 100;
    const DWORD WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT = 50;
    const DWORD WAIT_TIME_EVENTMONITOR_AFTER_WRITE_LONG = 150;

    const int READ_OUTPUT_RETRIES = 8;

    //
    // Level
    //
    constexpr static LPWSTR c_LevelToString[] =
    {
        L"None",
        L"Critical",
        L"Error",
        L"Warning",
        L"Information",
        L"Verbose",
    };

    WCHAR bigOutBuf[BUFFER_SIZE];
    int m_previousStdoutMode = -1;

    ///
    /// Gets the content of the Stdout buffer and returns it in a wstring.
    ///
    /// \return A wstring with the stdout.
    ///
    std::wstring RecoverOuput()
    {
        return std::wstring(bigOutBuf);
    }


    ///
    /// Writes an event
    ///
    /// \return Return true if the event was succesfully sent.
    ///
    int WriteEvent(EventChannelLogLevel Level,
                    int Id,
                    std::wstring Message)
    {
        if (Level <= EventChannelLogLevel::Critical || Level >= EventChannelLogLevel::Verbose)
        {
            return ERROR_INVALID_PARAMETER;
        }

        int status = _wsystem(
            Utility::FormatString(
                L"powershell -NoProfile -NonInteractive eventcreate /t %s /id %d /l APPLICATION /d \"\"\"\"%s\"\"\"\"",
                c_LevelToString[(int)Level],
                Id,
                Message.c_str()
            ).c_str()
        );

        return status;
    }


public:
    ///
    /// "Redirects" the stdout to our buffer.
    ///
    TEST_METHOD_INITIALIZE(InitializeEventMonitorTests)
    {
        //
        // Set our own buffer in stdout.
        //
        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
        fflush(stdout);
        m_previousStdoutMode = _setmode(_fileno(stdout), _O_U16TEXT);
        Assert::AreNotEqual(-1, m_previousStdoutMode);
        setvbuf(stdout, (char*)bigOutBuf, _IOFBF, sizeof(bigOutBuf) - sizeof(WCHAR));
    }

    TEST_METHOD_CLEANUP(CleanupEventMonitorTests)
    {
        fflush(stdout);
        setvbuf(stdout, nullptr, _IONBF, 0);
        if (m_previousStdoutMode != -1)
        {
            _setmode(_fileno(stdout), m_previousStdoutMode);
        }
    }

    TEST_METHOD(TestImmediateShutdown)
    {
        std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Error} };

        for (int attempt = 0; attempt < 20; ++attempt)
        {
            ULONGLONG start = GetTickCount64();
            {
                EventMonitor eventMonitor(eventChannels, true, false, L"json", L"");
            }
            Assert::IsTrue(GetTickCount64() - start < 5000,
                L"Stopping during startup must not exhaust the shutdown timeout.");
        }
    }

    TEST_METHOD(TestShutdownDuringReplay)
    {
        std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Error} };
        Assert::AreEqual(0, WriteEvent(EventChannelLogLevel::Error, 103, L"EventMonitor shutdown regression"));

        HANDLE batchReady = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        HANDLE releaseBatch = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        HANDLE shutdownRequested = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        Assert::IsNotNull(batchReady);
        Assert::IsNotNull(releaseBatch);
        Assert::IsNotNull(shutdownRequested);

        EventMonitorReplayTestHooks hooks{
            batchReady,
            releaseBatch,
            shutdownRequested,
            0,
            0,
            0,
        };
        auto eventMonitor = std::make_unique<EventMonitor>(
            eventChannels, true, true, L"json", L"", &hooks);

        DWORD batchWait = WaitForSingleObject(batchReady, 5000);
        if (batchWait != WAIT_OBJECT_0)
        {
            SetEvent(releaseBatch);
            eventMonitor.reset();
        }
        Assert::AreEqual(static_cast<DWORD>(WAIT_OBJECT_0), batchWait,
            L"Replay did not provide a batch before the test timeout.");
        Assert::IsTrue(hooks.batchSize > 0, L"Replay batch must contain an unprocessed event.");

        ULONGLONG start = GetTickCount64();
        std::thread shutdownThread([&eventMonitor]() { eventMonitor.reset(); });
        DWORD shutdownWait = WaitForSingleObject(shutdownRequested, 5000);
        SetEvent(releaseBatch);
        shutdownThread.join();

        Assert::AreEqual(static_cast<DWORD>(WAIT_OBJECT_0), shutdownWait,
            L"Destructor did not request shutdown before the test timeout.");
        Assert::IsTrue(GetTickCount64() - start < 5000,
            L"Shutdown must cancel replay rather than exhaust the shutdown timeout.");
        Assert::AreEqual(0L, hooks.processedEvents,
            L"Replay processed an event after shutdown was requested.");
        Assert::AreEqual(hooks.batchSize, hooks.closedEvents,
            L"Shutdown must close every unprocessed event handle in the replay batch.");

        CloseHandle(shutdownRequested);
        CloseHandle(releaseBatch);
        CloseHandle(batchReady);
    }

    TEST_METHOD(TestShutdownDoesNotLeakHandles)
    {
        std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Error} };
        {
            EventMonitor eventMonitor(eventChannels, true, false, L"json", L"");
            Sleep(WAIT_TIME_EVENTMONITOR_START);
        }

        DWORD handlesBefore = 0;
        Assert::IsTrue(GetProcessHandleCount(GetCurrentProcess(), &handlesBefore) != FALSE);
        for (int attempt = 0; attempt < 20; ++attempt)
        {
            EventMonitor eventMonitor(eventChannels, true, false, L"json", L"");
        }

        DWORD handlesAfter = 0;
        Assert::IsTrue(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) != FALSE);
        // Allow incidental test-host handles, but not two leaked handles per monitor.
        Assert::IsTrue(handlesAfter <= handlesBefore + 4,
            Utility::FormatString(L"Handle count grew from %lu to %lu.", handlesBefore, handlesAfter).c_str());
    }

    ///
    /// Check that EventMonitor prints the written errors when level <= Error.
    ///
    TEST_METHOD(TestErrorLevelMonitor)
    {
        std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Error} };

        EventMonitor eventMonitor(eventChannels, true, false, L"json", L"");
        Sleep(WAIT_TIME_EVENTMONITOR_START);

        //
        // Write an Error level event.
        //
        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring message = L"Hello world Error!";
            int eventId = 100;
            EventChannelLogLevel level = EventChannelLogLevel::Error;

            Assert::AreEqual(0, WriteEvent(level, eventId, message));

            std::wstring output;
            int count = 0;

            std::wregex rgxMessage(Utility::FormatString(L"\"Message\": \"%s\"", message.c_str()));

            do
            {
                Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

            Assert::IsTrue(std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            Assert::IsTrue(std::regex_search(output, rgxMessage),
                           Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxId(Utility::FormatString(L"\"EventId\": %d", eventId));

            Assert::IsTrue(std::regex_search(output, rgxId),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxLevel(Utility::FormatString(L"\"Level\": \"%s\"", c_LevelToString[(int)level]));

            Assert::IsTrue(std::regex_search(output, rgxLevel),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }

        //
        // Write an Warning level event.
        //
        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring message = L"Hello world Warning!";
            int eventId = 101;
            EventChannelLogLevel level = EventChannelLogLevel::Warning;

            Assert::AreEqual(0, WriteEvent(level, eventId, message));

            std::wstring output;

            Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_LONG);
            output = RecoverOuput();

            //
            // Monitor should have ignored this event.
            //
            std::wregex rgxMessage(Utility::FormatString(L"\"Message\": \"%s\"", message.c_str()));

            Assert::IsFalse(std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }

        //
        // Write an Information level event.
        //
        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring message = L"Hello world Info!";
            int eventId = 102;
            EventChannelLogLevel level = EventChannelLogLevel::Information;

            Assert::AreEqual(0, WriteEvent(level, eventId, message));

            std::wstring output;

            Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_LONG);
            output = RecoverOuput();

            //
            // Monitor should have ignored this event.
            //
            std::wregex rgxMessage(Utility::FormatString(L"\"Message\": \"%s\"", message.c_str()));

            Assert::IsFalse(std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }
    }

    ///
    /// Check that EventMonitor prints the written errors when level <= Information.
    ///
    TEST_METHOD(TestInformationLevelMonitor)
    {
        std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Information} };

        EventMonitor eventMonitor(eventChannels, true, false, L"json", L"");
        Sleep(WAIT_TIME_EVENTMONITOR_START);

        //
        // Write an Error level event.
        //
        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring message = L"Hello world Error!";
            int eventId = 100;
            EventChannelLogLevel level = EventChannelLogLevel::Error;

            Assert::AreEqual(0, WriteEvent(level, eventId, message));

            std::wstring output;
            int count = 0;

            std::wregex rgxMessage(Utility::FormatString(L"\"Message\": \"%s\"", message.c_str()));

            do
            {
                Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

            Assert::IsTrue(std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxId(Utility::FormatString(L"\"EventId\": %d", eventId));

            Assert::IsTrue(std::regex_search(output, rgxId),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxLevel(Utility::FormatString(L"\"Level\": \"%s\"", c_LevelToString[(int)level]));

            Assert::IsTrue(std::regex_search(output, rgxLevel),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }

        //
        // Write a Warning level event.
        //
        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring message = L"Hello world Warning!";
            int eventId = 101;
            EventChannelLogLevel level = EventChannelLogLevel::Warning;

            Assert::AreEqual(0, WriteEvent(level, eventId, message));

            std::wstring output;
            int count = 0;

            //
            // Test that the created event is printed. We could receive other events,
            // so is better to loop until our message has arrived, using a regex.
            //
            std::wregex rgxMessage(Utility::FormatString(L"\"Message\": \"%s\"", message.c_str()));

            do
            {
                Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

            Assert::IsTrue(std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxId(Utility::FormatString(L"\"EventId\": %d", eventId));

            Assert::IsTrue(std::regex_search(output, rgxId),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxLevel(Utility::FormatString(L"\"Level\": \"%s\"", c_LevelToString[(int)level]));

            Assert::IsTrue(std::regex_search(output, rgxLevel),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }

        //
        // Write an Information level event.
        //
        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring message = L"Hello world Info!";
            int eventId = 102;
            EventChannelLogLevel level = EventChannelLogLevel::Information;

            Assert::AreEqual(0, WriteEvent(level, eventId, message));

            std::wstring output;
            int count = 0;

            //
            // Test that the created event is printed. We could receive other events,
            // so is better to loop until our message has arrived, using a regex.
            //
            std::wregex rgxMessage(Utility::FormatString(L"\"Message\": \"%s\"", message.c_str()));

            do
            {
                Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

            Assert::IsTrue(std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxId(Utility::FormatString(L"\"EventId\": %d", eventId));

            Assert::IsTrue(std::regex_search(output, rgxId),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

            std::wregex rgxLevel(Utility::FormatString(L"\"Level\": \"%s\"", c_LevelToString[(int)level]));

            Assert::IsTrue(std::regex_search(output, rgxLevel),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }
    }

    //
    // Check that setting true the StartAtOldestRecord property, monitor
    // prints records that existed before the monitor started.
    //
    TEST_METHOD(TestStartAtOldestRecord)
    {
        std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Error} };

        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
        fflush(stdout);

        //
        // Write one application event, to ensure that there is at least
        // one event before starting the Event Monitor
        //
        std::wstring message = L"Hello world Info!";
        int eventId = 102;
        EventChannelLogLevel level = EventChannelLogLevel::Error;

        Assert::AreEqual(0, WriteEvent(level, eventId, message));

        EventMonitor eventMonitor(eventChannels, true, true, L"json", L"");
        Sleep(WAIT_TIME_EVENTMONITOR_START);

        {
            std::wstring output;
            int count = 0;
            do
            {
                Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && READ_OUTPUT_RETRIES > ++count);

            std::wregex rgxMessage(Utility::FormatString(L"\"Channel\": \"Application\""));

            Assert::IsTrue(std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }
    }

    //
    // Check that the events from non-listening channels are ignored.
    //
    TEST_METHOD(TestChannelsAreNotMixed)
    {
        std::vector<EventLogChannel> eventChannels = { {L"System", EventChannelLogLevel::Information} };

        EventMonitor eventMonitor(eventChannels, false, false, L"json", L"");
        Sleep(WAIT_TIME_EVENTMONITOR_START);

        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring message = L"Hello world Error!";
            int eventId = 100;
            EventChannelLogLevel level = EventChannelLogLevel::Error;

            Assert::AreEqual(0, WriteEvent(level, eventId, message));

            Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_LONG);
            std::wstring output = RecoverOuput();

            std::wregex rgxMessage(Utility::FormatString(L"\"Channel\": \"Application\""));

            //
            // We can not ensure that a System event isn't received right
            // now, so it would be enough checking that this is not
            // an appplication event.
            //
            Assert::IsTrue(!std::regex_search(output, rgxMessage),
                Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
        }
    }
};
}  // namespace LogMonitorTests
