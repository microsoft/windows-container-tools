//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "CppUnitTest.h"

#include <string>
#include <functional>
#include <wchar.h>
#include <memory>
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

#define TO_WSTR(Str) std::wstring(Str.begin(), Str.end())

namespace LogMonitorTests
{
	TEST_CLASS(LogFileMonitorTests)
	{
		const DWORD WAIT_TIME_LOGFILEMONITOR_START = 500;
		const DWORD WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE = 750;

		std::vector<std::wstring> directoriesToDeleteAtCleanup;

		WCHAR bigOutBuf[BUFFER_SIZE];
		
		std::wstring RecoverOuput()
		{
			return std::wstring(bigOutBuf);
		}

		std::wstring CreateTempDirectory()
		{
			WCHAR tempDirectory[L_tmpnam_s];
			ZeroMemory(tempDirectory, sizeof(tempDirectory));

			errno_t err = _wtmpnam_s(tempDirectory, L_tmpnam_s);
			if (err)
			{
				return L"";
			}

			long status = CreateDirectoryW(tempDirectory, NULL);
			if (status == 0)
			{
				return L"";
			}

			return std::wstring(tempDirectory);
		}

		DWORD WriteToFile(
			const std::wstring& FileName, 
			_In_reads_bytes_opt_(BufferSize) LPCVOID Buffer,
			_In_ DWORD BufferSize
		)
		{
			//
			// Open a handle to the file.
			//
			HANDLE hFile = CreateFile(
				FileName.c_str(),
				FILE_APPEND_DATA,
				FILE_SHARE_READ,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
				NULL);

			if (hFile == INVALID_HANDLE_VALUE)
			{
				//
				// Failed to open/create file.
				//
				return GetLastError();
			}

			//
			// Write data to the file.
			//
			DWORD bytesWritten;
			bool success = WriteFile(
				hFile,
				Buffer,
				BufferSize,
				&bytesWritten,
				nullptr);

			//
			// Close the handle once we don't need it.
			//
			CloseHandle(hFile);

			return (success)? 0 : GetLastError();
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

		TEST_METHOD_CLEANUP(CleanupLogFileMonitorTests)
		{
			for (auto directoryPath : directoriesToDeleteAtCleanup)
			{
				DWORD ftyp = GetFileAttributesW(directoryPath.c_str());
				if (ftyp & FILE_ATTRIBUTE_READONLY)
				{
					SetFileAttributes(directoryPath.c_str(), ftyp);
				}

				wchar_t* folder = new wchar_t[directoryPath.length() + 3];
				::ZeroMemory(folder, (directoryPath.length() + 3) * sizeof(folder[0]));

				size_t numCopied = directoryPath.copy(folder, directoryPath.length());

				SHFILEOPSTRUCT fileOp = {
				   NULL,
				   FO_DELETE,
				   folder,
				   NULL,
				   FOF_NOCONFIRMATION | FOF_NOERRORUI,
				   FALSE,
				   NULL,
				   NULL
				};

				long hr = SHFileOperation(&fileOp);

				delete[] folder;
			}
		}

		///
		/// Check that LogFileMonitor prints the updates of the files
		/// inside a directory, that is its core functionality.
		///
		TEST_METHOD(TestBasicLogFileMonitor)
		{
			std::wstring output;

			std::wstring tempDirectory = CreateTempDirectory();
			Assert::IsFalse(tempDirectory.empty());

			directoriesToDeleteAtCleanup.push_back(tempDirectory);

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = tempDirectory;

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Check that LogFileMonitor started successfully.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());
			{
				std::wstring filename = sourceFile.Directory + L"\\test.txt";
				std::string content = "Hello World!";

				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}
		}

		///
		/// Check that even if the directory doesn't exist yet, and it's created after
		/// some time, the monitor will read it successfully.
		///
		TEST_METHOD(TestDirectoryNotCreatedYet)
		{
			std::wstring output;

			WCHAR tempDirectoryArray[L_tmpnam_s];
			ZeroMemory(tempDirectoryArray, sizeof(tempDirectoryArray));

			errno_t err = _wtmpnam_s(tempDirectoryArray, L_tmpnam_s);
			Assert::IsFalse(err);

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = std::wstring(tempDirectoryArray) + L"NOT_CREATED_YET";

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Create the directory and wait to be recognized by the monitor. 
			//
			long status = CreateDirectoryW(sourceFile.Directory.c_str(), NULL);
			Sleep(7000);
			
			Assert::AreNotEqual(0L, status);

			//
			// Check that LogFileMonitor started successfully.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());
			{
				std::wstring filename = sourceFile.Directory + L"\\testfile.txt";
				std::string content = "Hello World!";

				status = WriteToFile(filename, content.c_str(), content.length());
				Assert::AreEqual(0L, status);
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE + 2000);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}
		}
		
		///
		/// Check that the filter specified to the monitor make it ignore all
		/// unmatched files.
		///
		TEST_METHOD(TestFilterIgnoreFiles)
		{
			std::wstring output;

			std::wstring tempDirectory = CreateTempDirectory();
			Assert::IsFalse(tempDirectory.empty());

			//
			// Create subdirectory
			//
			std::wstring subDirectory = tempDirectory + L"\\sub";
			long status = CreateDirectoryW(subDirectory.c_str(), NULL);
			Assert::AreNotEqual(0L, status);

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = tempDirectory;
			sourceFile.Filter = L"*.log";
			sourceFile.IncludeSubdirectories = true;

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Check that LogFileMonitor started successfully and print matched files.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());
			{
				std::wstring filename = sourceFile.Directory + L"\\test.log";
				std::string content = "Hello World!";

				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
			}

			
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = sourceFile.Directory + L"\\test.txt";
				std::string content = "Hello World!";

				//
				// Check unmatched new file is ignored.
				//
				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual(L"", output.c_str());

				//
				// Check unmatched existing file is ignored.
				//
				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual(L"", output.c_str());
			}

			//
			// Check the previous test, but for subdirectories.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = subDirectory + L"\\other_file.txt";
				std::string content = "Hello World!";

				//
				// Check unmatched new file is ignored.
				//
				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual(L"", output.c_str());

				//
				// Check unmatched existing file is ignored.
				//
				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual(L"", output.c_str());
			}
		}

		///
		/// Check that the decoding for the next encoding is working fine: 
		/// UTF8, UTF16 little endian an big endian, and ANSI.
		///
		TEST_METHOD(TestMultipleEncodings)
		{
			std::wstring output;

			std::wstring tempDirectory = CreateTempDirectory();
			Assert::IsFalse(tempDirectory.empty());

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = tempDirectory;

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Check that LogFileMonitor started successfully.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());

			//
			// Try UTF16 encoding without BOM.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = sourceFile.Directory + L"\\utf16.txt";
				std::wstring content = L"Hello world UTF16!";

				WriteToFile(filename, content.c_str(), content.length() * sizeof(WCHAR));
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((content + L"\n").c_str(), output.c_str());
			}

			//
			// Try UTF16 encoding with BOM.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = sourceFile.Directory + L"\\utf16Bom.txt";
				std::wstring content = L"Hello world UTF16 with BOM!";

				//
				// Add the BOM to the beginning of the file.
				//
				std::wstring contentWithBom = ((WCHAR)BYTE_ORDER_MARK) + content;

				WriteToFile(filename, contentWithBom.c_str(), contentWithBom.length() * sizeof(WCHAR));
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((content + L"\n").c_str(), output.c_str());
			}

			//
			// Try UTF16 big endian encoding with BOM.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = sourceFile.Directory + L"\\utf16BomBE.txt";
				std::wstring content = L"Hello world UTF16 with BOM and big endian!";

				//
				// Add the BOM to the beginning of the file.
				//
				std::wstring contentWithBom = ((WCHAR)BYTE_ORDER_MARK) + content;

				//
				// Reverse each wide character, to make it big endian
				//
				for (unsigned int i = 0; i < contentWithBom.size(); i++)
				{
					contentWithBom[i] = (TCHAR)(((contentWithBom[i] << 8) & 0xFF00) + ((contentWithBom[i] >> 8) & 0xFF));
				}

				WriteToFile(filename, contentWithBom.c_str(), contentWithBom.length() * sizeof(WCHAR));
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((content + L"\n").c_str(), output.c_str());
			}

			//
			// Try UTF8 encoding.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = sourceFile.Directory + L"\\utf8.txt";
				std::string content = "Hello world UTF8 \xe3\x83\x86!";

				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				std::wstring contentWide;
				int size_needed = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), content.length(), NULL, 0);
				contentWide.resize(size_needed);

				MultiByteToWideChar(CP_UTF8, 0, (LPCCH)content.c_str(), content.length(), (LPWSTR)(contentWide.c_str()), size_needed);

				output = RecoverOuput();
				Assert::AreEqual((contentWide + L"\n").c_str(), output.c_str());
			}

			//
			// Try ANSI encoding.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = sourceFile.Directory + L"\\ansi.txt";
				std::string content = "Hello world ANSI!";

				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}
		}

		///
		/// Check that adding files to the directory prints their content
		/// to stdout
		///
		TEST_METHOD(TestAddFiles)
		{
			std::wstring output;

			std::wstring tempDirectory = CreateTempDirectory();
			Assert::IsFalse(tempDirectory.empty());

			//
			// Create subdirectory
			//
			std::wstring subDirectory = tempDirectory + L"\\sub";
			long status = CreateDirectoryW(subDirectory.c_str(), NULL);
			Assert::AreNotEqual(0L, status);

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = tempDirectory;
			sourceFile.Filter = L"*.log";
			sourceFile.IncludeSubdirectories = true;

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Check that LogFileMonitor started successfully.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());

			//
			// Check if the short path could be doing something wrong.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				//
				// Try with a long file name
				//
				std::wstring filename = sourceFile.Directory + L"\\abcdefghijklmnopqrstuvwxyz.log";
				std::string content = "Hello World!";

				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}

			//
			// Check if the short path could be doing something wrong.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				//
				// Try with a long file name
				//
				std::wstring filename = subDirectory + L"\\other.log";
				std::string content = "Hello Subdirectory!";

				WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}
		}

		///
		/// Check that adding files to subdirectories works as same as
		/// adding it to the root directory.
		///
		TEST_METHOD(TestModifyFiles)
		{
			std::wstring output;

			std::wstring tempDirectory = CreateTempDirectory();
			Assert::IsFalse(tempDirectory.empty());

			//
			// Create a new file inside this directory.
			//
			std::wstring oldFilename = tempDirectory + L"\\oldfile.log";
			std::string oldContent = "Old content";

			WriteToFile(oldFilename, oldContent.c_str(), oldContent.length());

			//
			// Create subdirectory.
			//
			std::wstring subDirectory = tempDirectory + L"\\sub";
			long status = CreateDirectoryW(subDirectory.c_str(), NULL);
			Assert::AreNotEqual(0L, status);

			//
			// Create an file inside a subdirectory.
			//
			std::wstring oldFilenameSubdirectory = subDirectory + L"\\oldfile.log";
			WriteToFile(oldFilenameSubdirectory, oldContent.c_str(), oldContent.length());

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = tempDirectory;
			sourceFile.Filter = L"*.log";
			sourceFile.IncludeSubdirectories = true;

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Check that LogFileMonitor started successfully and that the content
			// of the existing files is not printed.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());

			//
			// Write to existing file and see stdout.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::string content = "New content";

				status = WriteToFile(oldFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}

			//
			// Write to existing file and see stdout.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::string content = "Other new content";

				WriteToFile(oldFilenameSubdirectory, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}

			//
			// Write a long line (more than 4096 characters).
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring filename = sourceFile.Directory + L"\\longline.log";
				std::string content(20000, '#');
				
				//
				// Add a new line to check that it doesn't break all.
				//
				content += '\n';

				status = WriteToFile(filename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content)).c_str(), output.c_str());
			}
		}

		///
		/// Check that renaming a file doesn't stop its listening.
		///
		TEST_METHOD(TestRenameFiles)
		{
			std::wstring output;

			std::wstring tempDirectory = CreateTempDirectory();
			Assert::IsFalse(tempDirectory.empty());

			//
			// Create a new file inside this directory.
			//
			std::wstring oldFilename = tempDirectory + L"\\oldfile.log";
			std::string oldContent = "Old content";

			WriteToFile(oldFilename, oldContent.c_str(), oldContent.length());

			//
			// Create subdirectory.
			//
			std::wstring subDirectory = tempDirectory + L"\\sub";
			long status = CreateDirectoryW(subDirectory.c_str(), NULL);
			Assert::AreNotEqual(0L, status);

			//
			// Create an file inside a subdirectory.
			//
			std::wstring oldFilenameSubdirectory = subDirectory + L"\\oldfile.log";
			WriteToFile(oldFilenameSubdirectory, oldContent.c_str(), oldContent.length());

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = tempDirectory;
			sourceFile.Filter = L"*.log";
			sourceFile.IncludeSubdirectories = true;

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Check that LogFileMonitor started successfully and that the content
			// of the existing files is not printed.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());

			//
			// Write to existing file and see stdout.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring newFilename = tempDirectory + L"\\renamedFile.log";

				bool success = MoveFile(oldFilename.c_str(), newFilename.c_str());
				Assert::IsTrue(success);

				std::string content = "New content";

				status = WriteToFile(newFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}

			//
			// Write to existing file and see stdout.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring newFilename = subDirectory + L"\\renamedFileSub.log";

				bool success = MoveFile(oldFilenameSubdirectory.c_str(), newFilename.c_str());
				Assert::IsTrue(success);

				std::string content = "Other new content";

				WriteToFile(newFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}

			//
			// Rename a matching file name to an unmatching one.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring initialFilename = tempDirectory + L"\\MatchingToUnmatching.log";
				std::wstring renamedFilename = tempDirectory + L"\\MatchingToUnmatching.txt";

				//
				// First, create an filter-matching file.
				//
				std::string content = "Other new content";
				WriteToFile(initialFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());

				//
				// Rename it with a filter-unmatching name.
				//
				bool success = MoveFile(initialFilename.c_str(), renamedFilename.c_str());
				Assert::IsTrue(success);

				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				WriteToFile(renamedFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				//
				// It should not be logged.
				//
				output = RecoverOuput();
				Assert::AreEqual(L"", output.c_str());
			}

			//
			// Rename a matching file name to an unmatching one.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring initialFilename = tempDirectory + L"\\UnmatchingToMatching.txt";
				std::wstring renamedFilename = tempDirectory + L"\\UnmatchingToMatching.log";

				//
				// First, create an filter-unmatching file.
				//
				std::string content = "Other new content";
				WriteToFile(initialFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				Assert::AreEqual(L"", output.c_str());

				//
				// Rename it with a filter-matching name.
				//
				bool success = MoveFile(initialFilename.c_str(), renamedFilename.c_str());
				Assert::IsTrue(success);

				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				WriteToFile(renamedFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				//
				// It should the FULL content of the file, that's why we need
				// to concatenate twice the content.
				//
				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}
		}

		///
		/// Check that renaming a subdirectory doesn't stop
		/// the listening to the files on it.
		///
		TEST_METHOD(TestRenameSubdirectories)
		{
			std::wstring output;

			std::wstring tempDirectory = CreateTempDirectory();
			Assert::IsFalse(tempDirectory.empty());

			//
			// Create subdirectory.
			//
			std::string oldContent = "Old content";
			std::wstring subDirectory = tempDirectory + L"\\sub";
			long status = CreateDirectoryW(subDirectory.c_str(), NULL);
			Assert::AreNotEqual(0L, status);

			//
			// Create an file inside a subdirectory.
			//
			std::wstring filename = L"\\filename.log";
			std::wstring oldFilenameSubdirectory = subDirectory + filename;
			WriteToFile(oldFilenameSubdirectory, oldContent.c_str(), oldContent.length());

			//
			// Start the monitor
			//
			SourceFile sourceFile;
			sourceFile.Directory = tempDirectory;
			sourceFile.Filter = L"*.log";
			sourceFile.IncludeSubdirectories = true;

			fflush(stdout);
			ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

			std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories);
			Sleep(WAIT_TIME_LOGFILEMONITOR_START);

			//
			// Check that LogFileMonitor started successfully and that the content
			// of the existing files is not printed.
			//
			output = RecoverOuput();
			Assert::AreEqual(L"", output.c_str());

			//
			// Write to existing file and see stdout.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::string content = "New content";

				WriteToFile(oldFilenameSubdirectory, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}

			//
			// Rename the subdirectory, write to the existing file insidte it
			// and see stdout.
			//
			{
				fflush(stdout);
				ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

				std::wstring newSubdirectory = tempDirectory + L"\\newSubdirectory";

				//
				// Rename the subdirectory.
				//
				bool success = MoveFile(subDirectory.c_str(), newSubdirectory.c_str());
				Assert::IsTrue(success);

				std::wstring newFilename = newSubdirectory + filename;

				//
				// Test that the file inside the renamed subdirectory still exists
				//
				WIN32_FIND_DATA FindFileData;
				HANDLE handle = FindFirstFileW(newFilename.c_str(), &FindFileData);
				Assert::IsTrue(handle != INVALID_HANDLE_VALUE);

				//
				// Write in it.
				//
				std::string content = "Other new content";

				WriteToFile(newFilename, content.c_str(), content.length());
				Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE);

				output = RecoverOuput();
				Assert::AreEqual((TO_WSTR(content) + L"\n").c_str(), output.c_str());
			}
		}
	};
}