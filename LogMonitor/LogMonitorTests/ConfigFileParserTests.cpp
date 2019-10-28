//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "CppUnitTest.h"

#include <string>
#include <functional>
#include <io.h> 
#include <fcntl.h> 

#include "../src/LogMonitor/LogWriter.h"
#include "../src/LogMonitor/EtwMonitor.h"
#include "../src/LogMonitor/EventMonitor.h"
#include "../src/LogMonitor/LogFileMonitor.h"
#include "../src/LogMonitor/ProcessMonitor.h"
#include "../src/LogMonitor/Utility.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#define BUFFER_SIZE 65536

namespace LogMonitorTests
{
	///
	/// Tests of the ConfigFileParser's ReadConfigFile function. This function 
	/// uses a JsonFileParser object to parse a JSON string, and return a
	/// vector of sources with the retrieved configuration.
	///
	TEST_CLASS(ConfigFileParserTests)
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

		///
		/// Replaces all the occurrences in a wstring. 
		///
		/// \param Str		The string to search substrings and replace them.
		/// \param From		The substring to being replaced.
		/// \param To		The substring to replace.
		///
		/// \return A wstring.
		///
		std::wstring ReplaceAll(std::wstring Str, const std::wstring& From, const std::wstring& To) {
			size_t start_pos = 0;
			
			while ((start_pos = Str.find(From, start_pos)) != std::string::npos) {
				Str.replace(start_pos, From.length(), To);
				start_pos += To.length(); // Handles case where 'To' is a substring of 'From'
			}
			return Str;
		}

		///
		/// Removes the braces at the start and end of a string. 
		///
		/// \param Str		A wstring.
		///
		/// \return A wstring.
		///
		std::wstring RemoveBracesGuidStr(const std::wstring& str)
		{
			if (str.size() >= 2 && str[0] == L'{' && str[str.length() - 1] == L'}')
			{
				return str.substr(1, str.length() - 2);
			}
			return str;
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
		/// Test that the most simple, but valid configuration string is
		/// read successfully.
		///
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

			std::wstring output = RecoverOuput();

			Assert::IsTrue(success);
			Assert::AreEqual(L"", output.c_str());
		}

		///
		/// Tests that EventLog sources, with all their attributes, are read
		/// successfully.
		///
		TEST_METHOD(TestSourceEventLog)
		{
			bool startAtOldestRecord = true;
			bool eventFormatMultiLine = true;

			//
			// Template of a valid configuration string, with an EventLog source.
			//
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

			//
			// Try reading this values
			//
			std::wstring firstChannelName = L"system";
			EventChannelLogLevel firstChannelLevel = EventChannelLogLevel::Information;

			std::wstring secondChannelName = L"application";
			EventChannelLogLevel secondChannelLevel = EventChannelLogLevel::Critical;
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

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::EventLog, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual(startAtOldestRecord, sourceEventLog->StartAtOldestRecord);
				Assert::AreEqual(eventFormatMultiLine, sourceEventLog->EventFormatMultiLine);

				Assert::AreEqual((size_t)2, sourceEventLog->Channels.size());

				Assert::AreEqual(firstChannelName.c_str(), sourceEventLog->Channels[0].Name.c_str());
				Assert::AreEqual((int)firstChannelLevel, (int)sourceEventLog->Channels[0].Level);

				Assert::AreEqual(secondChannelName.c_str(), sourceEventLog->Channels[1].Name.c_str());
				Assert::AreEqual((int)secondChannelLevel, (int)sourceEventLog->Channels[1].Level);
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

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::EventLog, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual(startAtOldestRecord, sourceEventLog->StartAtOldestRecord);
				Assert::AreEqual(eventFormatMultiLine, sourceEventLog->EventFormatMultiLine);

				Assert::AreEqual((size_t)2, sourceEventLog->Channels.size());

				Assert::AreEqual(firstChannelName.c_str(), sourceEventLog->Channels[0].Name.c_str());
				Assert::AreEqual((int)firstChannelLevel, (int)sourceEventLog->Channels[0].Level);

				Assert::AreEqual(secondChannelName.c_str(), sourceEventLog->Channels[1].Name.c_str());
				Assert::AreEqual((int)secondChannelLevel, (int)sourceEventLog->Channels[1].Level);
			}
		}

		///
		/// Test that default values for optional attributes on an EventLog source
		/// are correct.
		///
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
										\"name\": \"system\"\
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

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::EventLog, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual(false, sourceEventLog->StartAtOldestRecord);
				Assert::AreEqual(true, sourceEventLog->EventFormatMultiLine);

				Assert::AreEqual((size_t)1, sourceEventLog->Channels.size());

				Assert::AreEqual((int)EventChannelLogLevel::Error, (int)sourceEventLog->Channels[0].Level);
			}
		}

		///
		/// Tests that file sources, with all their attributes, are read
		/// successfully.
		///
		TEST_METHOD(TestSourceFile)
		{
			//
			// Template of a valid configuration string, with a file source.
			//
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

			//
			// First, try with this values.
			//
			bool includeSubdirectories = true;

			std::wstring directory = L"C:\\LogMonitor\\logs";
			std::wstring filter = L"*.*";
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
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::File, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(settings.Sources[0]);

				Assert::AreEqual(directory.c_str(), sourceFile->Directory.c_str());
				Assert::AreEqual(filter.c_str(), sourceFile->Filter.c_str());
				Assert::AreEqual(includeSubdirectories, sourceFile->IncludeSubdirectories);
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
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::File, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(settings.Sources[0]);

				Assert::AreEqual(directory.c_str(), sourceFile->Directory.c_str());
				Assert::AreEqual(filter.c_str(), sourceFile->Filter.c_str());
				Assert::AreEqual(includeSubdirectories, sourceFile->IncludeSubdirectories);
			}
		}

		///
		/// Test that default values for optional attributes on a file source are correct.
		///
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

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::File, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(settings.Sources[0]);

				Assert::AreEqual(directory.c_str(), sourceFile->Directory.c_str());
				Assert::AreEqual(L"", sourceFile->Filter.c_str());
				Assert::AreEqual(false, sourceFile->IncludeSubdirectories);
			}
		}

		///
		/// Tests that etw sources, with all their attributes, are read
		/// successfully.
		///
		TEST_METHOD(TestSourceETW)
		{
			HRESULT hr;

			//
			// Used to convert an etw log level value to its string representation.
			//
			const static std::vector<std::wstring> c_LevelToString =
			{
				L"Unknown",
				L"Critical",
				L"Error",
				L"Warning",
				L"Information",
				L"Verbose",
			};

			//
			// Template of a valid configuration string, with a file source.
			//
			std::wstring configFileStrFormat =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"ETW\",\
								\"eventFormatMultiLine\" : %s,\
								\"providers\" : [\
									{\
										\"providerName\": \"%s\",\
										\"providerGuid\": \"%s\",\
										\"level\" : \"%s\",\
										\"keywords\" : \"%llu\"\
									},\
									{\
										\"providerName\": \"%s\",\
										\"providerGuid\": \"%s\",\
										\"level\" : \"%s\",\
										\"keywords\" : \"%llu\"\
									}\
								]\
							}\
						]\
					}\
				}";

			//
			// First, try this values.
			//
			bool eventFormatMultiLine = true;

			std::wstring firstProviderName = L"IIS: WWW Server";
			std::wstring firstProviderGuid = L"3A2A4E84-4C21-4981-AE10-3FDA0D9B0F83";
			UCHAR firstProviderLevel = 2; // Error
			ULONGLONG firstProviderKeywords = 255;

			std::wstring secondProviderName = L"Microsoft-Windows-IIS-Logging";
			std::wstring secondProviderGuid = L"{7E8AD27F-B271-4EA2-A783-A47BDE29143B}";
			UCHAR secondProviderLevel = 1; // Critical
			ULONGLONG secondProviderKeywords = 555;

			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					eventFormatMultiLine ? L"true" : L"false",
					firstProviderName.c_str(),
					firstProviderGuid.c_str(),
					c_LevelToString[(int)firstProviderLevel].c_str(),
					firstProviderKeywords,
					secondProviderName.c_str(),
					secondProviderGuid.c_str(),
					c_LevelToString[(int)secondProviderLevel].c_str(),
					secondProviderKeywords
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::ETW, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceETW> sourceEtw = std::reinterpret_pointer_cast<SourceETW>(settings.Sources[0]);

				Assert::AreEqual(eventFormatMultiLine, sourceEtw->EventFormatMultiLine);

				Assert::AreEqual((size_t)2, sourceEtw->Providers.size());

				//
				// First provider
				//
				Assert::AreEqual(firstProviderName.c_str(), sourceEtw->Providers[0].ProviderName.c_str());
				Assert::AreEqual(firstProviderLevel, sourceEtw->Providers[0].Level);
				Assert::AreEqual(firstProviderKeywords, sourceEtw->Providers[0].Keywords);

				//
				// Check that guids are equal
				//
				LPWSTR providerGuid1 = NULL;
				hr = StringFromCLSID(sourceEtw->Providers[0].ProviderGuid, &providerGuid1);
				Assert::IsFalse(FAILED(hr));
				Assert::AreEqual(RemoveBracesGuidStr(firstProviderGuid).c_str(), RemoveBracesGuidStr(std::wstring(providerGuid1)).c_str());
				CoTaskMemFree(providerGuid1);

				//
				// Second provider
				//
				Assert::AreEqual(secondProviderName.c_str(), sourceEtw->Providers[1].ProviderName.c_str());
				Assert::AreEqual(secondProviderLevel, sourceEtw->Providers[1].Level);
				Assert::AreEqual(secondProviderKeywords, sourceEtw->Providers[1].Keywords);

				//
				// Check that guids are equal
				//
				LPWSTR providerGuid2 = NULL;
				hr = StringFromCLSID(sourceEtw->Providers[1].ProviderGuid, &providerGuid2);
				Assert::IsFalse(FAILED(hr));
				Assert::AreEqual(RemoveBracesGuidStr(secondProviderGuid).c_str(), RemoveBracesGuidStr(std::wstring(providerGuid2)).c_str());
				CoTaskMemFree(providerGuid2);
			}

			//
			// Try different values
			//
			eventFormatMultiLine = false;

			firstProviderName = L"Microsoft-Windows-SMBClient";
			firstProviderGuid = L"{988C59C5-0A1C-45B6-A555-0C62276E327D}";
			firstProviderLevel = 3; // Warning
			firstProviderKeywords = 0xff;

			secondProviderName = L"Microsoft-Windows-SMBWitnessClient";
			secondProviderGuid = L"32254F6C-AA33-46F0-A5E3-1CBCC74BF683";
			secondProviderLevel = 4; // Information
			secondProviderKeywords = 0xfe;

			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					eventFormatMultiLine ? L"true" : L"false",
					firstProviderName.c_str(),
					firstProviderGuid.c_str(),
					c_LevelToString[(int)firstProviderLevel].c_str(),
					firstProviderKeywords,
					secondProviderName.c_str(),
					secondProviderGuid.c_str(),
					c_LevelToString[(int)secondProviderLevel].c_str(),
					secondProviderKeywords
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::ETW, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceETW> sourceEtw = std::reinterpret_pointer_cast<SourceETW>(settings.Sources[0]);

				Assert::AreEqual(eventFormatMultiLine, sourceEtw->EventFormatMultiLine);

				Assert::AreEqual((size_t)2, sourceEtw->Providers.size());

				//
				// First provider
				//
				Assert::AreEqual(firstProviderName.c_str(), sourceEtw->Providers[0].ProviderName.c_str());
				Assert::AreEqual(firstProviderLevel, sourceEtw->Providers[0].Level);
				Assert::AreEqual(firstProviderKeywords, sourceEtw->Providers[0].Keywords);

				//
				// Check that guids are equal
				//
				LPWSTR providerGuid1 = NULL;
				hr = StringFromCLSID(sourceEtw->Providers[0].ProviderGuid, &providerGuid1);
				Assert::IsFalse(FAILED(hr));
				Assert::AreEqual(RemoveBracesGuidStr(firstProviderGuid).c_str(), RemoveBracesGuidStr(std::wstring(providerGuid1)).c_str());
				CoTaskMemFree(providerGuid1);

				//
				// Second provider
				//
				Assert::AreEqual(secondProviderName.c_str(), sourceEtw->Providers[1].ProviderName.c_str());
				Assert::AreEqual(secondProviderLevel, sourceEtw->Providers[1].Level);
				Assert::AreEqual(secondProviderKeywords, sourceEtw->Providers[1].Keywords);

				//
				// Check that guids are equal
				//
				LPWSTR providerGuid2 = NULL;
				hr = StringFromCLSID(sourceEtw->Providers[1].ProviderGuid, &providerGuid2);
				Assert::IsFalse(FAILED(hr));
				Assert::AreEqual(RemoveBracesGuidStr(secondProviderGuid).c_str(), RemoveBracesGuidStr(std::wstring(providerGuid2)).c_str());
				CoTaskMemFree(providerGuid2);
			}
		}

		///
		/// Test that default values for optional attributes on an etw source
		/// are correct.
		///
		TEST_METHOD(TestSourceETWDefaultValues)
		{
			HRESULT hr;
			const static std::vector<std::wstring> c_LevelToString =
			{
				L"Unknown",
				L"Critical",
				L"Error",
				L"Warning",
				L"Information",
				L"Verbose",
			};


			std::wstring firstProviderGuid = L"3A2A4E84-4C21-4981-AE10-3FDA0D9B0F83";

			std::wstring configFileStrFormat =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"ETW\",\
								\"providers\" : [\
									{\
										\"providerGuid\": \"%s\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					firstProviderGuid.c_str()
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::ETW, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceETW> sourceEtw = std::reinterpret_pointer_cast<SourceETW>(settings.Sources[0]);

				Assert::AreEqual(true, sourceEtw->EventFormatMultiLine);

				Assert::AreEqual((size_t)1, sourceEtw->Providers.size());

				//
				// First provider
				//
				Assert::AreEqual(L"", sourceEtw->Providers[0].ProviderName.c_str());
				Assert::AreEqual((UCHAR)2, sourceEtw->Providers[0].Level); // Error
				Assert::AreEqual((ULONGLONG)0L, sourceEtw->Providers[0].Keywords);

				//
				// Check that guids are equal
				//
				LPWSTR providerGuid1 = NULL;
				hr = StringFromCLSID(sourceEtw->Providers[0].ProviderGuid, &providerGuid1);
				Assert::IsFalse(FAILED(hr));
				Assert::AreEqual(RemoveBracesGuidStr(firstProviderGuid).c_str(), RemoveBracesGuidStr(std::wstring(providerGuid1)).c_str());
				CoTaskMemFree(providerGuid1);
			}

			std::wstring firstProviderName = L"Microsoft-Windows-User-Diagnostic";

			configFileStrFormat =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"ETW\",\
								\"providers\" : [\
									{\
										\"providerName\": \"%s\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				std::wstring configFileStr = Utility::FormatString(
					configFileStrFormat.c_str(),
					firstProviderName.c_str()
				);

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::ETW, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceETW> sourceEtw = std::reinterpret_pointer_cast<SourceETW>(settings.Sources[0]);

				Assert::AreEqual(true, sourceEtw->EventFormatMultiLine);

				Assert::AreEqual((size_t)1, sourceEtw->Providers.size());

				//
				// First provider
				//
				Assert::AreEqual(firstProviderName.c_str(), sourceEtw->Providers[0].ProviderName.c_str());
				Assert::AreEqual((UCHAR)2, sourceEtw->Providers[0].Level); // Error
				Assert::AreEqual((ULONGLONG)0L, sourceEtw->Providers[0].Keywords);
			}
		}

		///
		/// Test that ReadConfigFile reads attribute names in a case insensitive way.
		///
		TEST_METHOD(TestCaseInsensitiveOnAttributeNames)
		{
			std::wstring configFileStr =
				L"{	\
					\"logconfig\": {	\
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

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::EventLog, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);


				Assert::AreEqual((size_t)1, sourceEventLog->Channels.size());

				Assert::AreEqual(L"system", sourceEventLog->Channels[0].Name.c_str());
				Assert::AreEqual((int)EventChannelLogLevel::Verbose, (int)sourceEventLog->Channels[0].Level);
			}

			configFileStr =
				L"{	\
					\"LOGCONFIG\": {	\
						\"SourCes\": [ \
							{\
								\"Type\": \"EventLog\",\
								\"CHANNELS\" : [\
									{\
										\"Name\": \"system\",\
										\"Level\" : \"Verbose\"\
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

				std::wstring output = RecoverOuput();

				//
				// The config string was valid
				//
				Assert::IsTrue(success);
				Assert::AreEqual(L"", output.c_str());

				//
				// The source Event Log is valid
				//
				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::EventLog, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);


				Assert::AreEqual((size_t)1, sourceEventLog->Channels.size());

				Assert::AreEqual(L"system", sourceEventLog->Channels[0].Name.c_str());
				Assert::AreEqual((int)EventChannelLogLevel::Verbose, (int)sourceEventLog->Channels[0].Level);
			}
		}

		///
		/// Test that bad formatted JSON strings throw errors.
		///
		TEST_METHOD(TestInvalidJson)
		{
			std::wstring configFileStr;
			
			//
			// Empty string.
			//
			configFileStr =
				L"";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				std::function<void(void)> f1 = [&jsonParser, &settings] { bool success = ReadConfigFile(jsonParser, settings); };
				Assert::ExpectException<std::invalid_argument>(f1);
			}

			//
			// Invalid attribute name.
			//
			configFileStr =
				L"{other: false}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				std::function<void(void)> f1 = [&jsonParser, &settings] { bool success = ReadConfigFile(jsonParser, settings); };
				Assert::ExpectException<std::invalid_argument>(f1);
			}

			//
			// Invalid boolean value.
			//
			configFileStr =
				L"{\"boolean\": Negative}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				std::function<void(void)> f1 = [&jsonParser, &settings] { bool success = ReadConfigFile(jsonParser, settings); };
				Assert::ExpectException<std::invalid_argument>(f1);
			}

			//
			// Invalid numeric value.
			//
			configFileStr =
				L"{\"numeric\": 0xff}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				std::function<void(void)> f1 = [&jsonParser, &settings] { bool success = ReadConfigFile(jsonParser, settings); };
				Assert::ExpectException<std::invalid_argument>(f1);
			}

			//
			// Invalid escape sequence.
			//
			configFileStr =
				L"{\"text\": \"\\k\"}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				std::function<void(void)> f1 = [&jsonParser, &settings] { bool success = ReadConfigFile(jsonParser, settings); };
				Assert::ExpectException<std::invalid_argument>(f1);
			}

			//
			// Expected next element on an object.
			//
			configFileStr =
				L"{\"text\": \"\",}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				std::function<void(void)> f1 = [&jsonParser, &settings] { bool success = ReadConfigFile(jsonParser, settings); };
				Assert::ExpectException<std::invalid_argument>(f1);
			}

			//
			// Expected next element on an array.
			//
			configFileStr =
				L"{\"array\":[\"text\": \"\",]}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				std::function<void(void)> f1 = [&jsonParser, &settings] { bool success = ReadConfigFile(jsonParser, settings); };
				Assert::ExpectException<std::invalid_argument>(f1);
			}
		}

		///
		/// Test that valid JSON strings, but invalid values for a configuration string,
		/// return false when passed to ReadConfigFile.
		///
		TEST_METHOD(TestInvalidConfigFile)
		{
			std::wstring configFileStr;

			//
			// 'LogConfig' root element doesn't exist.
			//
			configFileStr =
				L"{\"other\": { }}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsFalse(success);
			}

			//
			// LogConfig is an array.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": []\
				}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsFalse(success);
			}

			//
			// 'Sources' doesn't exist.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"other\": [ \
							{\
								\"type\": \"File\",\
								\"directory\": \"C:\\\\logs\"\
							}\
						]\
					}\
				}";
			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsFalse(success);
			}

			//
			// 'sources' isn't an array.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": \
							{\
								\"type\": \"File\",\
								\"directory\": \"C:\\\\logs\"\
							}\
					}\
				}";

			{
				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsFalse(success);
			}

			//
			// Check a source with invalid type.
			// This case is special, because it shouldn't return false, but a 
			// warning message should be throw.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"Unknown\",\
								\"directory\": \"C:\\\\logs\"\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::IsTrue(output.find(L"ERROR") != std::wstring::npos);
			}
		}

		///
		/// Check that invalid EventLog sources are not returned by ReadConfigFile.
		///
		TEST_METHOD(TestInvalidEventLogSource)
		{
			std::wstring configFileStr;

			//
			// 'Channels' doesn't exist.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"EventLog\",\
								\"other\" : [\
									{\
										\"name\": \"system\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)0, settings.Sources.size());
				Assert::IsTrue(output.find(L"ERROR") != std::wstring::npos);
			}

			//
			// 'Channels' isn't an array.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"EventLog\",\
								\"channels\" : \
									{\
										\"name\": \"system\"\
									}\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)0, settings.Sources.size());
				Assert::IsTrue(output.find(L"ERROR") != std::wstring::npos);
			}

			//
			// Invalid channel. It should have at least a 'name' attribute.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"EventLog\",\
								\"channels\" : [\
									{\
										\"other\": \"system\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::EventLog, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual((size_t)0, sourceEventLog->Channels.size());
				Assert::IsTrue(output.find(L"WARNING") != std::wstring::npos);
			}

			//
			// Invalid level.
			// This case is special, because the source should be returned, but a 
			// warning message should be throw.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"EventLog\",\
								\"channels\" : [\
									{\
										\"name\": \"system\",\
										\"level\": \"Invalid\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::EventLog, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(settings.Sources[0]);

				Assert::AreEqual((size_t)1, sourceEventLog->Channels.size());
				Assert::AreEqual((int)EventChannelLogLevel::Error, (int)sourceEventLog->Channels[0].Level);
				Assert::IsTrue(output.find(L"WARNING") != std::wstring::npos);
			}
		}

		///
		/// Check that invalid File sources are not returned by ReadConfigFile.
		///
		TEST_METHOD(TestInvalidFileSource)
		{
			std::wstring configFileStr;

			//
			// 'Directory' doesn't exist
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"File\",\
								\"other\": \"C:\\\\logs\"\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)0, settings.Sources.size());
				Assert::IsTrue(output.find(L"ERROR") != std::wstring::npos);
			}
		}

		///
		/// Check that invalid ETW sources are not returned by ReadConfigFile.
		///
		TEST_METHOD(TestInvalidETWSource)
		{
			std::wstring configFileStr;

			//
			// 'providers' doesn't exist
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"ETW\",\
								\"other\" : [\
									{\
										\"providerGuid\": \"305FC87B-002A-5E26-D297-60223012CA9C\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)0, settings.Sources.size());
				Assert::IsTrue(output.find(L"ERROR") != std::wstring::npos);
			}

			//
			// Invalid provider
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"ETW\",\
								\"providers\" : [\
									{\
										\"level\": \"Information\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::ETW, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceETW> SourceEtw = std::reinterpret_pointer_cast<SourceETW>(settings.Sources[0]);

				Assert::AreEqual((size_t)0, SourceEtw->Providers.size());
				Assert::IsTrue(output.find(L"WARNING") != std::wstring::npos);
			}

			//
			// Invalid providerGuid.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"ETW\",\
								\"providers\" : [\
									{\
										\"providerGuid\": \"305FC87B-002A-5E26-D297-60\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::ETW, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceETW> SourceEtw = std::reinterpret_pointer_cast<SourceETW>(settings.Sources[0]);

				Assert::AreEqual((size_t)0, SourceEtw->Providers.size());
				Assert::IsTrue(output.find(L"WARNING") != std::wstring::npos);
			}

			//
			// Invalid level.
			// This case is special, because the source should be returned, but a 
			// warning message should be throw.
			//
			configFileStr =
				L"{	\
					\"LogConfig\": {	\
						\"sources\": [ \
							{\
								\"type\": \"ETW\",\
								\"providers\" : [\
									{\
										\"providerGuid\": \"305FC87B-002A-5E26-D297-60223012CA9C\",\
										\"level\": \"Info\"\
									}\
								]\
							}\
						]\
					}\
				}";

			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				JsonFileParser jsonParser(configFileStr);
				LoggerSettings settings;

				bool success = ReadConfigFile(jsonParser, settings);
				Assert::IsTrue(success);

				std::wstring output = RecoverOuput();

				Assert::AreEqual((size_t)1, settings.Sources.size());
				Assert::AreEqual((int)LogSourceType::ETW, (int)settings.Sources[0]->Type);

				std::shared_ptr<SourceETW> SourceEtw = std::reinterpret_pointer_cast<SourceETW>(settings.Sources[0]);

				Assert::AreEqual((size_t)1, SourceEtw->Providers.size());
				Assert::AreEqual((UCHAR)2, SourceEtw->Providers[0].Level); // Error
				Assert::IsTrue(output.find(L"WARNING") != std::wstring::npos);
			}
		}
	};
}
