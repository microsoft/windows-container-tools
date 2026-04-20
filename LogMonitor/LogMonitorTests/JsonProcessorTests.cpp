//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "../src/LogMonitor/JsonProcessor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#define BUFFER_SIZE 65536

namespace LogMonitorTests
{
    ///
    /// Tests of the JsonProcessor's ReadConfigFile function, which replaced the
    /// Boost-based ConfigFileParser. Covers source parsing, optional-field
    /// defaults, case-insensitive key matching, waitInSeconds, and invalid-input
    /// rejection.
    ///
    TEST_CLASS(JsonProcessorTests)
    {
        WCHAR bigOutBuf[BUFFER_SIZE];
        std::wstring tempFilePath;

        std::wstring RecoverOutput()
        {
            return std::wstring(bigOutBuf);
        }

        std::wstring WriteTempConfig(const std::string& content)
        {
            WCHAR tempDir[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tempDir);
            WCHAR tempFile[MAX_PATH] = {};
            GetTempFileNameW(tempDir, L"jpt", 0, tempFile);
            tempFilePath = tempFile;

            std::ofstream f(tempFile, std::ios::binary);
            f << content;
            return tempFilePath;
        }

    public:

        TEST_METHOD_INITIALIZE(Initialize)
        {
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);
            _setmode(_fileno(stdout), _O_U16TEXT);
            setvbuf(stdout, (char*)bigOutBuf, _IOFBF, sizeof(bigOutBuf) - sizeof(WCHAR));
            tempFilePath.clear();
        }

        TEST_METHOD_CLEANUP(Cleanup)
        {
            if (!tempFilePath.empty())
            {
                DeleteFileW(tempFilePath.c_str());
            }
        }

        ///
        /// Channel name/level lookup must be case-insensitive for backward
        /// compatibility with existing configs.
        ///
        TEST_METHOD(JsonProcessor_HandlesCaseInsensitiveKeys)
        {
            auto path = WriteTempConfig(R"({
                "LogConfig": {
                    "sources": [{
                        "type": "EventLog",
                        "channels": [{"Name": "system", "Level": "Warning"}]
                    }]
                }
            })");

            LoggerSettings settings;
            bool success = ReadConfigFile((PWCHAR)path.c_str(), settings);

            Assert::IsTrue(success);
            Assert::AreEqual((size_t)1, settings.Sources.size());

            auto src = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);
            Assert::AreEqual(std::wstring(L"system"), src->Channels[0].Name);
            Assert::AreEqual((int)EventChannelLogLevel::Warning, (int)src->Channels[0].Level);
        }

        ///
        /// Optional fields omitted from the config must use their documented
        /// default values so existing configs are not broken.
        ///
        TEST_METHOD(JsonProcessor_UsesDefaultValuesForOptionalFields)
        {
            // EventLog: startAtOldestRecord defaults false, eventFormatMultiLine
            // defaults true, channel level defaults Error.
            auto path = WriteTempConfig(R"({
                "LogConfig": {
                    "sources": [{
                        "type": "EventLog",
                        "channels": [{"name": "system"}]
                    }]
                }
            })");

            LoggerSettings settings;
            bool success = ReadConfigFile((PWCHAR)path.c_str(), settings);

            Assert::IsTrue(success);
            Assert::AreEqual((size_t)1, settings.Sources.size());

            auto src = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);
            Assert::IsFalse(src->StartAtOldestRecord);
            Assert::IsTrue(src->EventFormatMultiLine);
            Assert::AreEqual((int)EventChannelLogLevel::Error, (int)src->Channels[0].Level);
        }

        ///
        /// waitInSeconds on a File source must be parsed and stored correctly.
        /// When omitted the default value (300) must be used.
        ///
        TEST_METHOD(JsonProcessor_ParsesWaitInSeconds)
        {
            // Explicit value
            auto path = WriteTempConfig(R"({
                "LogConfig": {
                    "sources": [{
                        "type": "File",
                        "directory": "C:\\logs",
                        "filter": "*.log",
                        "includeSubdirectories": false,
                        "waitInSeconds": 60
                    }]
                }
            })");

            LoggerSettings settings;
            bool success = ReadConfigFile((PWCHAR)path.c_str(), settings);

            Assert::IsTrue(success);
            Assert::AreEqual((size_t)1, settings.Sources.size());

            auto src = std::reinterpret_pointer_cast<SourceFile>(settings.Sources[0]);
            Assert::AreEqual(60.0, src->WaitInSeconds);

            // Default value when omitted
            auto path2 = WriteTempConfig(R"({
                "LogConfig": {
                    "sources": [{
                        "type": "File",
                        "directory": "C:\\logs",
                        "filter": "*.log",
                        "includeSubdirectories": false
                    }]
                }
            })");

            LoggerSettings settings2;
            bool success2 = ReadConfigFile((PWCHAR)path2.c_str(), settings2);

            Assert::IsTrue(success2);
            auto src2 = std::reinterpret_pointer_cast<SourceFile>(settings2.Sources[0]);
            Assert::AreEqual(300.0, src2->WaitInSeconds);
        }

        ///
        /// A source with an unknown type must be skipped with an error logged,
        /// but valid sources in the same config must still be processed.
        ///
        TEST_METHOD(JsonProcessor_RejectsInvalidSource)
        {
            auto path = WriteTempConfig(R"({
                "LogConfig": {
                    "sources": [
                        {"type": "InvalidSourceType"},
                        {
                            "type": "File",
                            "directory": "C:\\logs",
                            "filter": "*.log",
                            "includeSubdirectories": false
                        }
                    ]
                }
            })");

            LoggerSettings settings;
            bool success = ReadConfigFile((PWCHAR)path.c_str(), settings);

            // Overall parse succeeds — invalid source is skipped, valid one is kept
            Assert::IsTrue(success);
            Assert::AreEqual((size_t)1, settings.Sources.size());
            Assert::AreEqual((int)LogSourceType::File, (int)settings.Sources[0]->Type);
        }
    };
}
