//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"


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
        const DWORD WAIT_TIME_EVENTMONITOR_EXIT = 500;

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
                    L"powershell eventcreate /t %s /id %d /l APPLICATION /d \"\"\"\"%s\"\"\"\"",
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
            _setmode(_fileno(stdout), _O_U16TEXT);
            setvbuf(stdout, (char*)bigOutBuf, _IOFBF, sizeof(bigOutBuf) - sizeof(WCHAR));
        }

        TEST_METHOD_CLEANUP(CleanupEventMonitorTests)
        {
            Sleep(WAIT_TIME_EVENTMONITOR_EXIT);
        }

        ///
        /// Check that EventMonitor prints the written errors when level <= Error.
        ///
        TEST_METHOD(TestErrorLevelMonitor)
        {
            std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Error} };

            EventMonitor eventMonitor(eventChannels, true, false);
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
                
                std::wregex rgxMessage(Utility::FormatString(L"<Message>%s<\\/Message>", message.c_str()));

                do
                {
                    Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                    output = RecoverOuput();
                } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

                Assert::IsTrue(std::regex_search(output, rgxMessage),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                Assert::IsTrue(std::regex_search(output, rgxMessage),
                               Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxId(Utility::FormatString(L"<EventId>%d<\\/EventId>", eventId));

                Assert::IsTrue(std::regex_search(output, rgxId),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxLevel(Utility::FormatString(L"<Level>%s<\\/Level>", c_LevelToString[(int)level]));

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
                std::wregex rgxMessage(Utility::FormatString(L"<Message>%s<\\/Message>", message.c_str()));

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
                std::wregex rgxMessage(Utility::FormatString(L"<Message>%s<\\/Message>", message.c_str()));

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

            EventMonitor eventMonitor(eventChannels, true, false);
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
                
                std::wregex rgxMessage(Utility::FormatString(L"<Message>%s<\\/Message>", message.c_str()));

                do
                {
                    Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                    output = RecoverOuput();
                } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

                Assert::IsTrue(std::regex_search(output, rgxMessage),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxId(Utility::FormatString(L"<EventId>%d<\\/EventId>", eventId));

                Assert::IsTrue(std::regex_search(output, rgxId),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxLevel(Utility::FormatString(L"<Level>%s<\\/Level>", c_LevelToString[(int)level]));

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
                std::wregex rgxMessage(Utility::FormatString(L"<Message>%s<\\/Message>", message.c_str()));

                do
                {
                    Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                    output = RecoverOuput();
                } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

                Assert::IsTrue(std::regex_search(output, rgxMessage),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxId(Utility::FormatString(L"<EventId>%d<\\/EventId>", eventId));

                Assert::IsTrue(std::regex_search(output, rgxId),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxLevel(Utility::FormatString(L"<Level>%s<\\/Level>", c_LevelToString[(int)level]));

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
                std::wregex rgxMessage(Utility::FormatString(L"<Message>%s<\\/Message>", message.c_str()));

                do
                {
                    Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                    output = RecoverOuput();
                } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

                Assert::IsTrue(std::regex_search(output, rgxMessage),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxId(Utility::FormatString(L"<EventId>%d<\\/EventId>", eventId));

                Assert::IsTrue(std::regex_search(output, rgxId),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                std::wregex rgxLevel(Utility::FormatString(L"<Level>%s<\\/Level>", c_LevelToString[(int)level]));

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

            EventMonitor eventMonitor(eventChannels, true, true);
            Sleep(WAIT_TIME_EVENTMONITOR_START);

            {
                std::wstring output;
                int count = 0;
                do
                {
                    Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                    output = RecoverOuput();
                } while (output.empty() && READ_OUTPUT_RETRIES > ++count);

                std::wregex rgxMessage(Utility::FormatString(L"<Channel>Application<\\/Channel>"));

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

            EventMonitor eventMonitor(eventChannels, false, false);
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

                std::wregex rgxMessage(Utility::FormatString(L"<Channel>Application<\\/Channel>"));

                //
                // We can not ensure that a System event isn't received right
                // now, so it would be enough checking that this is not
                // an appplication event.
                //
                Assert::IsTrue(!std::regex_search(output, rgxMessage),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
            }
        }

        ///
        /// Check that setting false the EventFormatMultiline property, monitor
        /// prints white spaces instead of new lines.
        ///
        TEST_METHOD(TestEventFormatMultiline)
        {
            std::vector<EventLogChannel> eventChannels = { {L"Application", EventChannelLogLevel::Information} };

            EventMonitor eventMonitor(eventChannels, false, false);
            Sleep(WAIT_TIME_EVENTMONITOR_START);

            {
                ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
                fflush(stdout);

                int eventId = 555;
                EventChannelLogLevel level = EventChannelLogLevel::Error;

                //
                // The way to insert a new line in powershell is using `r`n.
                //
                std::wstring message = L"Hello world`r`nError!";
                
                //
                // This should be the sanitized message.
                //
                std::wstring messageWithoutNewline = L"Hello world  Error!";


                Assert::AreEqual(0, WriteEvent(level, eventId, message));

                std::wstring output;
                int count = 0;

                //
                // Test that the created event is printed. We could receive other events,
                // so is better to loop until our message has arrived, using a regex.
                //
                std::wregex rgxMessage(Utility::FormatString(L"<Message>%s<\\/Message>", messageWithoutNewline.c_str()));

                do
                {
                    Sleep(WAIT_TIME_EVENTMONITOR_AFTER_WRITE_SHORT);
                    output = RecoverOuput();
                } while (!std::regex_search(output, rgxMessage) && READ_OUTPUT_RETRIES > ++count);

                Assert::IsTrue(std::regex_search(output, rgxMessage),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
            }
        }
    };
}