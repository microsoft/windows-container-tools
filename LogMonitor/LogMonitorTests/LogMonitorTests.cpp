//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

#include "../src/LogMonitor/ConfigFileParser.cpp"
#include "../src/LogMonitor/EtwMonitor.cpp"
#include "../src/LogMonitor/EventMonitor.cpp"
#include "../src/LogMonitor/JsonFileParser.cpp"
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
			setvbuf(stdout, (char*)bigOutBuf, _IOFBF, sizeof(bigOutBuf));
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
