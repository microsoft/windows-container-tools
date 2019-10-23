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

		std::wstring ReplaceAll(std::wstring str, const std::wstring& from, const std::wstring& to) {
			size_t start_pos = 0;
			
			while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
				str.replace(start_pos, from.length(), to);
				start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
			}
			return str;
		}

	public:

		TEST_METHOD_INITIALIZE(InitializeConfigFileParserTests)
		{
			//
			// Set our own buffer in stdout
			//
			ZeroMemory(bigOutBuf, BUFFER_SIZE);
			fflush(stdout);
			setvbuf(stdout, bigOutBuf, _IOFBF, BUFFER_SIZE);
		}

		TEST_METHOD(TestBasicConfigFile)
		{
			std::wstring configFileStr = 
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
						]\
					}\
				}";

			JsonFileParser jsonParser(configFileStr);
			LoggerSettings settings;

			bool success = ReadConfigFile(jsonParser, settings);

			wstring output = RecoverOuput();

			Assert::IsTrue(success);
			Assert::AreEqual(output.size(), (size_t)0);
		}

		TEST_METHOD(TestSourceEventLog)
		{
			bool startAtOldestRecord = true;
			bool eventFormatMultiLine = true;

			std::wstring firstChannelName = L"system";
			EventChannelLogLevel firstChannelLevel = EventChannelLogLevel::Information;

			std::wstring secondChannelName = L"application";
			EventChannelLogLevel secondChannelLevel = EventChannelLogLevel::Critical;

			std::wstring configFileStrFormat =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"EventLog\",\
								\"startAtOldestRecord\" : %s,\
								\"eventFormatMultiLine\" : %s,\
								\"channels\" : [\
									{\
										\"name\": \"%s\",\
										\"level\" : \"%s\"\
									},\
									{\
										\"name\": \"%s\",\
										\"level\" : \"%s\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					startAtOldestRecord ? L"true" : L"false",
					eventFormatMultiLine ? L"true" : L"false",
					firstChannelName.c_str(),
					LogLevelNames[(int)firstChannelLevel - 1].c_str(),
					secondChannelName.c_str(),
					LogLevelNames[(int)secondChannelLevel - 1].c_str()
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(output.size(), (size_t)0);

				//
				// The source Event Log is valid
				//
				Assert::AreEqual(settings.Sources.size(), (size_t)1);
				Assert::AreEqual((int)settings.Sources[0]->Type, (int)LogSourceType::EventLog);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual(sourceEventLog->StartAtOldestRecord, startAtOldestRecord);
				Assert::AreEqual(sourceEventLog->EventFormatMultiLine, eventFormatMultiLine);

				Assert::AreEqual(sourceEventLog->Channels.size(), (size_t)2);

				Assert::AreEqual(sourceEventLog->Channels[0].Name.c_str(), firstChannelName.c_str());
				Assert::AreEqual((int)sourceEventLog->Channels[0].Level, (int)firstChannelLevel);

				Assert::AreEqual(sourceEventLog->Channels[1].Name.c_str(), secondChannelName.c_str());
				Assert::AreEqual((int)sourceEventLog->Channels[1].Level, (int)secondChannelLevel);
			}

			//
			// Try with different values
			//
			startAtOldestRecord = false;
			eventFormatMultiLine = false;

			firstChannelName = L"security";
			firstChannelLevel = EventChannelLogLevel::Error;

			secondChannelName = L"kernel";
			secondChannelLevel = EventChannelLogLevel::Warning;

			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					startAtOldestRecord ? L"true" : L"false",
					eventFormatMultiLine ? L"true" : L"false",
					firstChannelName.c_str(),
					LogLevelNames[(int)firstChannelLevel - 1].c_str(),
					secondChannelName.c_str(),
					LogLevelNames[(int)secondChannelLevel - 1].c_str()
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(output.size(), (size_t)0);

				//
				// The source Event Log is valid
				//
				Assert::AreEqual(settings.Sources.size(), (size_t)1);
				Assert::AreEqual((int)settings.Sources[0]->Type, (int)LogSourceType::EventLog);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual(sourceEventLog->StartAtOldestRecord, startAtOldestRecord);
				Assert::AreEqual(sourceEventLog->EventFormatMultiLine, eventFormatMultiLine);

				Assert::AreEqual(sourceEventLog->Channels.size(), (size_t)2);

				Assert::AreEqual(sourceEventLog->Channels[0].Name.c_str(), firstChannelName.c_str());
				Assert::AreEqual((int)sourceEventLog->Channels[0].Level, (int)firstChannelLevel);

				Assert::AreEqual(sourceEventLog->Channels[1].Name.c_str(), secondChannelName.c_str());
				Assert::AreEqual((int)sourceEventLog->Channels[1].Level, (int)secondChannelLevel);
			}
		}

		TEST_METHOD(TestSourceEventLogDefaultValues)
		{
			std::wstring configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"EventLog\",\
								\"channels\" : [\
									{\
										\"name\": \"system\",\
										\"level\" : \"Verbose\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(output.size(), (size_t)0);

				//
				// The source Event Log is valid
				//
				Assert::AreEqual(settings.Sources.size(), (size_t)1);
				Assert::AreEqual((int)settings.Sources[0]->Type, (int)LogSourceType::EventLog);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual(sourceEventLog->StartAtOldestRecord, false);
				Assert::AreEqual(sourceEventLog->EventFormatMultiLine, true);

				Assert::AreEqual(sourceEventLog->Channels.size(), (size_t)1);
			}
		}

		TEST_METHOD(TestSourceFile)
		{
			bool includeSubdirectories = true;

			std::wstring directory = L"C:\\LogMonitor\\logs";
			std::wstring filter = L"*.*";

			std::wstring configFileStrFormat =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"File\",\
								\"directory\": \"%s\",\
								\"filter\": \"%s\",\
								\"includeSubdirectories\" : %s\
							}\
						]\
					}\
				}";
			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					ReplaceAll(directory, L"\\", L"\\\\").c_str(),
					filter.c_str(),
					includeSubdirectories ? L"true" : L"false"
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(output.size(), (size_t)0);

				//
				// The source Event Log is valid
				//
				Assert::AreEqual(settings.Sources.size(), (size_t)1);
				Assert::AreEqual((int)settings.Sources[0]->Type, (int)LogSourceType::File);

				std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(settings.Sources[0]);

				Assert::AreEqual(sourceFile->Directory.c_str(), directory.c_str());
				Assert::AreEqual(sourceFile->Filter.c_str(), filter.c_str());
				Assert::AreEqual(sourceFile->IncludeSubdirectories, includeSubdirectories);
			}

			//
			// Try with different values
			//
			includeSubdirectories = false;

			directory = L"c:\\\\inetpub\\\\logs";
			filter = L"*.log";

			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					ReplaceAll(directory, L"\\", L"\\\\").c_str(),
					filter.c_str(),
					includeSubdirectories ? L"true" : L"false"
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(output.size(), (size_t)0);

				//
				// The source Event Log is valid
				//
				Assert::AreEqual(settings.Sources.size(), (size_t)1);
				Assert::AreEqual((int)settings.Sources[0]->Type, (int)LogSourceType::File);

				std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(settings.Sources[0]);

				Assert::AreEqual(sourceFile->Directory.c_str(), directory.c_str());
				Assert::AreEqual(sourceFile->Filter.c_str(), filter.c_str());
				Assert::AreEqual(sourceFile->IncludeSubdirectories, includeSubdirectories);
			}
		}

		TEST_METHOD(TestSourceFileDefaultValues)
		{

			std::wstring directory = L"C:\\LogMonitor\\logs";

			std::wstring configFileStrFormat =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"File\",\
								\"directory\": \"%s\"\
							}\
						]\
					}\
				}";

			std::wstring configFileStr = Utility::FormatString(
				configFileStrFormat.c_str(),
				ReplaceAll(directory, L"\\", L"\\\\").c_str()
			);

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(output.size(), (size_t)0);

				//
				// The source Event Log is valid
				//
				Assert::AreEqual(settings.Sources.size(), (size_t)1);
				Assert::AreEqual((int)settings.Sources[0]->Type, (int)LogSourceType::File);

				std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(settings.Sources[0]);

				Assert::AreEqual(sourceFile->Directory.c_str(), directory.c_str());
				Assert::AreEqual(sourceFile->Filter.c_str(), L"");
				Assert::AreEqual(sourceFile->IncludeSubdirectories, false);
			}
		}
	};
}
