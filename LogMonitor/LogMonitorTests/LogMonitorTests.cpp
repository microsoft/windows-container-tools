//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

#include "../src/LogMonitor/EtwMonitor.cpp"
#include "../src/LogMonitor/EventMonitor.cpp"
#include "../src/LogMonitor/JsonFileParser.cpp"
#include "../src/LogMonitor/JsonProcessor.cpp"
#include "../src/LogMonitor/FileMonitor/FileMonitorUtilities.cpp"
#include "../src/LogMonitor/LogFileMonitor.cpp"
#include "../src/LogMonitor/ProcessMonitor.cpp"
#include "../src/LogMonitor/Utility.cpp"

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "ws2_32.lib")  // For ntohs function
#pragma comment(lib, "shlwapi.lib") 

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

LogWriter logWriter;

#define BUFFER_SIZE 65536

namespace LogMonitorTests
{
    TEST_CLASS(EventMessageFormattingTests)
    {
        struct FormatReply
        {
            DWORD capacity;
            BOOL success;
            DWORD error;
            DWORD used;
            std::wstring text;
            bool terminated = true;
        };

        inline static thread_local std::vector<FormatReply> replies;
        inline static thread_local size_t nextReply = 0;

        static BOOL WINAPI FormatMessage(
            EVT_HANDLE, EVT_HANDLE, DWORD messageId, DWORD valueCount,
            PEVT_VARIANT values, DWORD flags, DWORD bufferSize,
            LPWSTR buffer, PDWORD used)
        {
            Assert::IsTrue(nextReply < replies.size(), L"Unexpected formatting call.");
            const auto& reply = replies[nextReply++];
            Assert::AreEqual(0UL, messageId);
            Assert::AreEqual(0UL, valueCount);
            Assert::IsTrue(values == nullptr);
            Assert::AreEqual(static_cast<DWORD>(EvtFormatMessageEvent), flags);
            Assert::AreEqual(reply.capacity, bufferSize);
            if (bufferSize == 0)
            {
                Assert::IsTrue(buffer == nullptr);
            }
            else
            {
                Assert::IsTrue(buffer != nullptr);
                Assert::IsTrue(reply.text.size() + (reply.terminated ? 1 : 0) <= bufferSize);
                std::fill(buffer, buffer + bufferSize, L'!');
                std::copy(reply.text.begin(), reply.text.end(), buffer);
                if (reply.terminated)
                {
                    buffer[reply.text.size()] = L'\0';
                }
            }
            *used = reply.used;
            SetLastError(reply.error);
            return reply.success;
        }

        static void CheckFormatting(
            const std::vector<FormatReply>& sequence,
            DWORD expectedStatus,
            DWORD expectedError,
            const std::wstring& expectedText)
        {
            replies = sequence;
            nextReply = 0;
            std::vector<wchar_t> message(256, L'z');
            DWORD formattingError = ERROR_SUCCESS;
            DWORD status = FormatEventMessage(nullptr, nullptr, message, formattingError, FormatMessage);
            Assert::AreEqual(expectedStatus, status);
            Assert::AreEqual(expectedError, formattingError);
            Assert::AreEqual(expectedText, std::wstring(message.begin(), message.end()));
            Assert::IsTrue(nextReply == replies.size(), L"Expected another formatting call.");
        }

    public:
        TEST_METHOD(TestSuccessfulMessageIgnoresLastErrorAndOldBufferContents)
        {
            CheckFormatting({
                {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 16, L""},
                {16, TRUE, ERROR_ACCESS_DENIED, 3, L"ok"},
            }, ERROR_SUCCESS, ERROR_SUCCESS, L"ok");
        }

        TEST_METHOD(TestPreservesPartialMessages)
        {
            for (DWORD error : {
                ERROR_EVT_UNRESOLVED_VALUE_INSERT,
                ERROR_EVT_UNRESOLVED_PARAMETER_INSERT,
                ERROR_EVT_MAX_INSERTS_REACHED,
                ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND})
            {
                CheckFormatting({
                    {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 5, L""},
                    {5, FALSE, error, 5, L"%2f!"},
                }, ERROR_SUCCESS, error, L"%2f!");
            }
        }

        TEST_METHOD(TestRetriesLargerBuffer)
        {
            CheckFormatting({
                {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 1, L""},
                {1, FALSE, ERROR_INSUFFICIENT_BUFFER, 3, L""},
                {3, TRUE, ERROR_SUCCESS, 3, L"ok"},
            }, ERROR_SUCCESS, ERROR_SUCCESS, L"ok");
        }

        TEST_METHOD(TestRetriesAfterSizingResourceError)
        {
            CheckFormatting({
                {0, FALSE, ERROR_MUI_FILE_NOT_FOUND, 0, L""},
                {1, FALSE, ERROR_INSUFFICIENT_BUFFER, 5, L""},
                {5, FALSE, ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND, 5, L"%2f!"},
            }, ERROR_SUCCESS, ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND, L"%2f!");
        }

        TEST_METHOD(TestInsufficientBufferWithoutGrowthFails)
        {
            CheckFormatting({
                {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 3, L""},
                {3, FALSE, ERROR_INSUFFICIENT_BUFFER, 3, L""},
            }, ERROR_INSUFFICIENT_BUFFER, ERROR_INSUFFICIENT_BUFFER, L"");
        }

        TEST_METHOD(TestFailedFormattingDiscardsPreviousMessage)
        {
            for (DWORD error : {ERROR_ACCESS_DENIED, ERROR_EVT_MESSAGE_NOT_FOUND})
            {
                CheckFormatting({
                    {0, FALSE, error, 0, L""},
                    {1, FALSE, error, 0, L""},
                }, error, error, L"");
            }

            CheckFormatting({
                {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 5, L""},
                {5, FALSE, ERROR_ACCESS_DENIED, 5, L"text"},
            }, ERROR_ACCESS_DENIED, ERROR_ACCESS_DENIED, L"");
        }

        TEST_METHOD(TestPartialErrorWithoutTextIsNotSuccess)
        {
            CheckFormatting({
                {0, FALSE, ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND, 0, L""},
                {1, FALSE, ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND, 0, L""},
            }, ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND, ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND, L"");

            CheckFormatting({
                {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 1, L""},
                {1, FALSE, ERROR_EVT_UNRESOLVED_VALUE_INSERT, 1, L""},
            }, ERROR_EVT_UNRESOLVED_VALUE_INSERT, ERROR_EVT_UNRESOLVED_VALUE_INSERT, L"");
        }

        TEST_METHOD(TestRejectsInvalidMessageSizeOrTermination)
        {
            CheckFormatting({
                {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 3, L""},
                {3, TRUE, ERROR_SUCCESS, 4, L"ok"},
            }, ERROR_INVALID_DATA, ERROR_SUCCESS, L"");

            CheckFormatting({
                {0, FALSE, ERROR_INSUFFICIENT_BUFFER, 3, L""},
                {3, TRUE, ERROR_SUCCESS, 3, L"bad", false},
            }, ERROR_INVALID_DATA, ERROR_SUCCESS, L"");
        }
    };

    ///
    /// Tests the tools that other tests could use. i.e. redirecting stdout.
    ///
    TEST_CLASS(LogMonitorTests)
    {
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

    public:

        ///
        /// "Redirects" the stdout to our buffer. 
        ///
        TEST_METHOD_INITIALIZE(InitializeLogFileMonitorTests)
        {
            //
            // Set our own buffer in stdout.
            //
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);
            _setmode(_fileno(stdout), _O_U16TEXT);
            setvbuf(stdout, (char*)bigOutBuf, _IOFBF, sizeof(bigOutBuf) - sizeof(WCHAR));
        }
        
        ///
        /// Test that things being printed with logWriter are sent to
        /// bigOutBuf buffer. 
        ///
        TEST_METHOD(TestRedirectedOutputForTests)
        {
            std::wstring expectedOutput(BUFFER_SIZE - 4, L'#');

            logWriter.WriteConsoleLog(expectedOutput);
            expectedOutput += L"\n";

            //
            // Recover the stdout
            //
            std::wstring realOutput = RecoverOuput();

            Assert::AreEqual(expectedOutput, realOutput);
        }
    };
}
