//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"  // NOLINT(build/include_subdir)
#include <wrl/client.h>  // NOLINT(build/include_order)
#include <xmllite.h>  // NOLINT(build/include_order)
#include <cstdio>  // NOLINT(build/include_order)
#include <string>  // NOLINT(build/include_order)

// The VS test project compiles these implementations into this test translation unit.
// NOLINTBEGIN(build/include)
#include "../src/LogMonitor/EtwMonitor.cpp"
#include "../src/LogMonitor/EventMonitor.cpp"
#include "../src/LogMonitor/JsonFileParser.cpp"
#include "../src/LogMonitor/JsonProcessor.cpp"
#include "../src/LogMonitor/FileMonitor/FileMonitorUtilities.cpp"
#include "../src/LogMonitor/LogFileMonitor.cpp"
#include "../src/LogMonitor/ProcessMonitor.cpp"
#include "../src/LogMonitor/Utility.cpp"
// NOLINTEND

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "ws2_32.lib")  // For ntohs function
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "xmllite.lib")

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

LogWriter logWriter;

#define BUFFER_SIZE 65536

namespace LogMonitorTests
{
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

TEST_CLASS(ProcessMonitorTests)
{
    std::wstring previousLogFormat;
    std::wstring previousCustomFormat;

    const std::string utf8Message =
        "ASCII \xc2\xa1 \xe2\x82\xac \xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x98\x80 \xef\xbf\xbd";

    void CheckStandardFormats(
        const std::string& input, const std::string& expectedJson, const std::string& expectedXml)
    {
        loggingformat = L"JSON";
        std::string formatted = FormatProcessLog(input);
        Assert::AreEqual(
            std::string("{\"Source\":\"Process\",\"LogEntry\":{\"Logline\":\"") +
                expectedJson + "\"},\"SchemaVersion\":\"1.0.0\"}\n",
            formatted);
        nlohmann::json parsed = nlohmann::json::parse(formatted);
        Assert::AreEqual(expectedJson, parsed.at("LogEntry").at("Logline").get<std::string>());

        loggingformat = L"XML";
        formatted = FormatProcessLog(input);
        Microsoft::WRL::ComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(
            reinterpret_cast<const BYTE*>(formatted.data()), static_cast<UINT>(formatted.size())));
        Assert::IsNotNull(stream.Get());
        Microsoft::WRL::ComPtr<IXmlReader> reader;
        Assert::IsTrue(SUCCEEDED(CreateXmlReader(IID_PPV_ARGS(reader.GetAddressOf()), nullptr)));
        Assert::IsTrue(SUCCEEDED(reader->SetInput(stream.Get())));
        XmlNodeType nodeType;
        HRESULT status;
        while ((status = reader->Read(&nodeType)) == S_OK)
        {
        }
        Assert::IsTrue(status == S_FALSE, L"Process output must be well-formed XML.");
        Assert::AreEqual(
            std::string("<Log><Source>Process</Source><LogEntry><Logline>") +
                expectedXml + "</Logline></LogEntry></Log>\n",
            formatted);
    }

    void CheckStandardFormats(const std::string& input, const std::string& expected)
    {
        CheckStandardFormats(input, expected, expected);
    }

public:
    TEST_METHOD_INITIALIZE(InitializeProcessMonitorTests)
    {
        previousLogFormat = loggingformat;
        previousCustomFormat = processCustomLogFormat;
    }

    TEST_METHOD_CLEANUP(CleanupProcessMonitorTests)
    {
        loggingformat = previousLogFormat;
        processCustomLogFormat = previousCustomFormat;
    }

    TEST_METHOD(TestStandardFormatsPreserveUtf8)
    {
        CheckStandardFormats(utf8Message, utf8Message);
        std::string longMessage = std::string(4095, 'a') + utf8Message;
        CheckStandardFormats(longMessage, longMessage);
    }

    TEST_METHOD(TestStandardFormatsHandleXmlNoncharacters)
    {
        for (const auto& noncharacter : {"\xef\xbf\xbe", "\xef\xbf\xbf"})
        {
            std::string input = utf8Message + noncharacter + utf8Message;
            CheckStandardFormats(input, input, utf8Message + "???" + utf8Message);
        }
        CheckStandardFormats("\xef\xbf\xbe\xef\xbf\xbf", "\xef\xbf\xbe\xef\xbf\xbf", "??????");
        const std::string allowed = "\xef\xbf\xbd\xef\xb7\x90\xf0\x9f\xbf\xbe\xf4\x8f\xbf\xbf";
        CheckStandardFormats(allowed, allowed);
    }

    TEST_METHOD(TestStandardFormatsPreserveAsciiAndNullReplacement)
    {
        CheckStandardFormats("", "");
        CheckStandardFormats("plain ASCII 123", "plain ASCII 123");
        std::string withNull = "before";
        withNull.push_back('\0');
        withNull += "after";
        CheckStandardFormats(withNull, "before?after");
    }

    TEST_METHOD(TestStandardFormatsReplaceInvalidUtf8)
    {
        struct TestCase
        {
            const char* input;
            const char* expected;
        };
        const TestCase cases[] = {
            {"\x80", "?"},
            {"\xff", "?"},
            {"\xc0\xaf", "??"},
            {"\xed\xa0\x80", "???"},
            {"\xf4\x90\x80\x80", "????"},
            {"\xe2\x28\xa1", "?(?"},
            {"\xf0\x9f\x98", "???"},
        };
        for (const auto& testCase : cases)
        {
            CheckStandardFormats(testCase.input, testCase.expected);
        }
        CheckStandardFormats(utf8Message + "\xff" + utf8Message,
            utf8Message + "?" + utf8Message);
    }

    TEST_METHOD(TestCustomFormatPreservesUtf8)
    {
        loggingformat = L"Custom";
        processCustomLogFormat = L"%Source%: %Message%";
        const std::string input = utf8Message + "\xef\xbf\xbe\xef\xbf\xbf";
        Assert::AreEqual(std::string("Process: ") + input + "\n", FormatProcessLog(input));
    }
};
}  // namespace LogMonitorTests
