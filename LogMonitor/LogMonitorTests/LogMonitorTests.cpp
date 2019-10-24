//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "CppUnitTest.h"

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <winevt.h>

#include "../src/LogMonitor/LogWriter.h"
#include "../src/LogMonitor/EtwMonitor.h"
#include "../src/LogMonitor/EventMonitor.h"
#include "../src/LogMonitor/LogFileMonitor.h"
#include "../src/LogMonitor/ProcessMonitor.h"
#include "../src/LogMonitor/Utility.h"
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
		char bigOutBuf[BUFFER_SIZE];

		///
		/// Gets the content of the Stdout buffer and returns it in a wstring. 
		///
		/// \return A wstring with the stdout.
		///
		std::wstring RecoverOuput()
		{
			std::string realOutputStr(bigOutBuf);
			return std::wstring(realOutputStr.begin(), realOutputStr.end());
		}

	public:

		///
		/// "Redirects" the stdout to our buffer. 
		///
		TEST_METHOD_INITIALIZE(InitializeLogMonitorTests)
		{
			//
			// Set our own buffer in stdout
			//
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
			fflush(stdout);
			setvbuf(stdout, bigOutBuf, _IOFBF, BUFFER_SIZE);
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
