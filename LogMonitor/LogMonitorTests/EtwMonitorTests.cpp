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
    /// Tests of ETW Monitor. This class listens "Microsoft-Windows-User-Diagnostic"
    /// provider and tests that the Monitor prints the events to stdout.
    ///
    TEST_CLASS(EtwMonitorTests)
    {
        ///
        /// The waiting time for monitor operations
        ///
        const DWORD WAIT_TIME_ETWMONITOR_START = 3000;

        const int READ_OUTPUT_RETRIES = 4;

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
        TEST_METHOD_INITIALIZE(InitializeEtwMonitorTests)
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
        /// Check that EtwMonitor receives events from "Microsoft-Windows-User-Diagnostic"
        /// provider.
        ///
        TEST_METHOD(TestEtwMonitorReceivesEvents)
        {
            ETWProvider provider;
            provider.ProviderName = L"Microsoft-Windows-User-Diagnostic";
            provider.SetProviderGuid(L"305FC87B-002A-5E26-D297-60223012CA9C");
            provider.Level = 3; // Warning
            provider.Keywords = 0;

            std::vector<ETWProvider> etwProviders = { provider };
            {
                //
                // Start ETW Monitor
                //
                ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
                fflush(stdout);

                EtwMonitor etwMonitor(etwProviders, true);
                Sleep(WAIT_TIME_ETWMONITOR_START);

                
                std::wstring output = RecoverOuput();

                //
                // The "Microsoft-Windows-User-Diagnostic" provider produces events
                // like this:
                //
                //  <Source>EtwEvent</Source><Time>2019-11-04T18:04:33.000Z</Time>
                //  <Provider idGuid="{305FC87B-002A-5E26-D297-60223012CA9C}"/>
                //  <DecodingSource>DecodingSourceXMLFile</DecodingSource>
                //  <Execution ProcessID="3968" ThreadID="2504" /><Level>Warning</Level>
                //  <Keyword>0x0</Keyword><EventID Qualifiers="1">1</EventID>
                //  <EventData><ErrorCode>0x3F0</ErrorCode></EventData>
                //

                //
                // Verify time was printed.
                //
                std::wregex rgxTime(L"<Time>\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}.\\d{3}Z<\\/Time>");

                Assert::IsTrue(std::regex_search(output, rgxTime),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                //
                // Verify provider GUID was printed.
                //
                std::wregex rgxProvider(L"<Provider[^>]*[\\da-fA-F]{8}-[\\da-fA-F]{4}-[\\da-fA-F]{4}-[\\da-fA-F]{4}-[\\da-fA-F]{12}");

                Assert::IsTrue(std::regex_search(output, rgxProvider),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                //
                // Verify level was printed.
                //
                std::wregex rgxLevel(L"<Level>(None|Critical|Error|Warning|Information|Verbose)<\\/Level>");

                Assert::IsTrue(std::regex_search(output, rgxLevel),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                //
                // Verify keyword was printed.
                //
                std::wregex rgxKeyword(L"<Keyword>0x[\\da-fA-F]+<\\/Keyword>");

                Assert::IsTrue(std::regex_search(output, rgxKeyword),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());

                //
                // Verify event data was printed.
                //
                std::wregex rgxData(L"<EventData>.*<ErrorCode>0x[\\da-fA-F]+<\\/ErrorCode>.*<\\/EventData>");

                Assert::IsTrue(std::regex_search(output, rgxData),
                    Utility::FormatString(L"Actual output: %s", output.c_str()).c_str());
            }
        }

        ///
        /// Check that EtwMonitor can search for a provider, using only its name.
        ///
        TEST_METHOD(TestEtwMonitorOnlyProviderName)
        {
            ETWProvider provider;
            provider.ProviderName = L"Microsoft-Windows-User-Diagnostic";
            provider.Level = 3; // Warning
            provider.Keywords = 0;

            std::vector<ETWProvider> etwProviders = { provider };

            //
            // Start ETW Monitor
            //
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            EtwMonitor etwMonitor(etwProviders, true);

            //
            // It must find the provider, and start printing events.
            //
            std::wstring output;
            int count = 0;
            do {
                Sleep(WAIT_TIME_ETWMONITOR_START);
                output = RecoverOuput();
            } while (output.empty() && ++count < READ_OUTPUT_RETRIES);

            Assert::AreNotEqual(L"", output.c_str());
        }

        ///
        /// Check that, if the provider name doesn't exist in the system, the
        /// monitor's constructor fails.
        ///
        TEST_METHOD(TestEtwMonitorInvalidProviderName)
        {
            ETWProvider provider;
            provider.ProviderName = L"Non-existing provider";
            provider.Level = 3; // Warning
            provider.Keywords = 0;

            std::vector<ETWProvider> etwProviders = { provider };

            //
            // Start ETW Monitor
            //
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);


            std::function<void(void)> f1 = [&etwProviders] { EtwMonitor etwMonitor(etwProviders, true); };
            Assert::ExpectException<std::invalid_argument>(f1);
        }

        ///
        /// Check that EtwMonitor level defaults to Error when level is not given
        ///
        TEST_METHOD(TestEtwMonitorNoLevelProvided)
        {
            //provider whose level is specified as error
            ETWProvider providerWithLevel;
            providerWithLevel.ProviderName = L"Microsoft-Windows-User-Diagnostic";
            providerWithLevel.Level = 2; // Error
            providerWithLevel.Keywords = 0;

            ETWProvider providerWithoutLevel;
            providerWithoutLevel.ProviderName = L"Microsoft-Windows-User-Diagnostic";
            providerWithoutLevel.Keywords = 0;

            std::vector<ETWProvider> etwProviders = { providerWithLevel, providerWithoutLevel };
            EtwMonitor etwMonitor(etwProviders, true);

            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            std::wstring output1, output2;
            int count1 = 0, count2 = 0;
            do {
                Sleep(WAIT_TIME_ETWMONITOR_START);
                output1 = RecoverOuput();
            } while (output1.empty() && ++count1 < READ_OUTPUT_RETRIES);

            
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            fflush(stdout);

            do {
                Sleep(WAIT_TIME_ETWMONITOR_START);
                output2 = RecoverOuput();
            } while (output2.empty() && ++count2 < READ_OUTPUT_RETRIES);


            //assert that both outputs are the equal.
            Assert::AreEqual(output1.c_str(), output2.c_str());
        }
    };
}