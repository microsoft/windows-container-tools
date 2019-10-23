#include "pch.h"
#include "CppUnitTest.h"

#include <algorithm>

#include "../src/LogMonitor/LogWriter.h"
#include "../src/LogMonitor/ConfigFileParser.cpp"
#include "../src/LogMonitor/Utility.cpp"
#include "../src/LogMonitor/JsonFileParser.cpp"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#define BUFFER_SIZE 65536

namespace LogMonitorTests
{
	TEST_CLASS(ConfigFileParserTests)
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
			std::wstring configFileStr = 
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"EventLog\",\
								\"Channels\" : [\
									{\
										\"name\": \"system\",\
										\"level\" : \"Information\"\
									}\
								]\
							}\
						]\
					}\
				}";

			JsonFileParser jsonParser(configFileStr);
			LoggerSettings settings;

			bool success = false;
			
			success = ReadConfigFile(jsonParser, settings);

			Logger::WriteMessage(RecoverOuput().c_str());

			Assert::IsTrue(success);
		}
	};
}
