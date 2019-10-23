#include "pch.h"
#include "CppUnitTest.h"

#include "../src/LogMonitor/LogWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

LogWriter logWriter;

#define BUFFER_SIZE 65536

namespace LogMonitorTests
{
	TEST_CLASS(LogMonitorTests)
	{
		char bigOutBuf[BUFFER_SIZE];

		std::wstring RecoverOuput()
		{
			std::string realOutputStr(bigOutBuf);
			return std::wstring(realOutputStr.begin(), realOutputStr.end());
		}

	public:

		TEST_METHOD_INITIALIZE(InitializeLogMonitorTests)
		{
			//
			// Set our own buffer in stdout
			//
			ZeroMemory(bigOutBuf, BUFFER_SIZE);
			fflush(stdout);
			setvbuf(stdout, bigOutBuf, _IOFBF, BUFFER_SIZE);
		}
		
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
