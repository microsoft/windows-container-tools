//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"  // NOLINT(build/include_subdir)
#include <cstdio>  // NOLINT(build/include_order)
#include <cstring>  // NOLINT(build/include_order)
#include <memory>  // NOLINT(build/include_order)
#include <string>  // NOLINT(build/include_order)
#include <vector>  // NOLINT(build/include_order)


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#define BUFFER_SIZE 65536

#define TO_WSTR(Str) std::wstring(Str.begin(), Str.end())

namespace LogMonitorTests
{
///
/// Tests the Log File Monitor. This class creates temporaries directories
/// and files, and tests that the Monitor prints all the changes to stdout.
///
TEST_CLASS(LogFileMonitorTests)
{
    ///
    /// The wait time after each modification to a subdirectory or a file.
    ///
    const DWORD WAIT_TIME_LOGFILEMONITOR_START = 500;
    const DWORD WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_LONG = 750;
    const DWORD WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT = 500;

    const int READ_OUTPUT_RETRIES = 8;

    ///
    /// Add the path of the created directories, to be removed during
    /// cleanup.
    ///
    std::vector<std::wstring> directoriesToDeleteAtCleanup;

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
    /// Writes to a new or existing file.
    ///
    /// \param FileName      The full path of the file.
    /// \param FileName      The buffer with the content to write.
    /// \param Buffer        The size of the buffer in bytes.
    ///
    ///
    /// \return The status of the operation.
    ///
    DWORD WriteToFile(
        const std::wstring& FileName,
        _In_reads_bytes_opt_(BufferSize) LPCVOID Buffer,
        _In_ size_t BufferSize
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
            (DWORD)BufferSize,
            &bytesWritten,
            nullptr);

        //
        // Close the handle once we don't need it.
        //
        CloseHandle(hFile);

        return (success)? 0 : GetLastError();
    }

    void StopMonitor(LogFileMonitor& Monitor)
    {
        Assert::IsTrue(SetEvent(Monitor.m_stopEvent) != FALSE);
        Assert::AreEqual(DWORD(WAIT_OBJECT_0), WaitForSingleObject(Monitor.m_logDirMonitorThread, 5000));
        Assert::AreEqual(DWORD(WAIT_OBJECT_0), WaitForSingleObject(Monitor.m_logFilesChangeHandlerThread, 5000));
    }

    void WaitForOutput(const std::wstring& Marker)
    {
        int retries = 0;
        while (RecoverOuput().find(Marker) == std::wstring::npos && retries++ < READ_OUTPUT_RETRIES)
        {
            Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
        }
        Assert::IsTrue(RecoverOuput().find(Marker) != std::wstring::npos, Marker.c_str());
    }

    void AddTrackedFile(
        LogFileMonitor& Monitor, const std::wstring& LongPath, const std::wstring& ShortPath, DWORD Id)
    {
        auto info = std::make_shared<LogFileInformation>();
        info->FileName = LongPath;
        FILE_ID_INFO fileId = {};
        memcpy(fileId.FileId.Identifier, &Id, sizeof(Id));
        info->ShortName = ShortPath;
        info->FileId = fileId;
        Monitor.m_logFilesInformation[LongPath] = info;
        Monitor.m_longPaths[ShortPath] = LongPath;
        Monitor.m_fileIds[fileId] = LongPath;
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

    TEST_METHOD_CLEANUP(CleanupLogFileMonitorTests)
    {
        for (auto directoryPath : directoriesToDeleteAtCleanup)
        {
            DWORD ftyp = GetFileAttributesW(directoryPath.c_str());
            if (ftyp & FILE_ATTRIBUTE_READONLY)
            {
                SetFileAttributes(directoryPath.c_str(), ftyp);
            }

            //
            // Add null charachters for null termination and for second
            // null term. required by SHFILEOPSTRUCT.
            //
            wchar_t* folder = new wchar_t[directoryPath.length() + 3];
            ::ZeroMemory(folder, (directoryPath.length() + 3) * sizeof(folder[0]));

            directoryPath.copy(folder, directoryPath.length());

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

            SHFileOperation(&fileOp);

            delete[] folder;
        }
    }

    TEST_METHOD(TestRemovedDirectoryClearsTrackedFiles)
    {
        for (bool moveBack : {false, true})
        {
            fflush(stdout);
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));
            const std::wstring tempDirectory = CreateTempDirectory();
            const std::wstring outsideDirectory = CreateTempDirectory();
            Assert::IsFalse(tempDirectory.empty());
            Assert::IsFalse(outsideDirectory.empty());
            directoriesToDeleteAtCleanup.push_back(tempDirectory);
            directoriesToDeleteAtCleanup.push_back(outsideDirectory);

            const std::wstring removedDirectory = tempDirectory + L"\\RemovedDirectory";
            const std::wstring nestedDirectory = removedDirectory + L"\\Nested";
            const std::wstring retainedDirectory = tempDirectory + L"\\RemovedDirectoryOther";
            const std::wstring movedDirectory = outsideDirectory + L"\\MovedDirectory";
            Assert::IsTrue(CreateDirectoryW(removedDirectory.c_str(), nullptr) != FALSE);
            Assert::IsTrue(CreateDirectoryW(nestedDirectory.c_str(), nullptr) != FALSE);
            Assert::IsTrue(CreateDirectoryW(retainedDirectory.c_str(), nullptr) != FALSE);

            const std::wstring removedFile = removedDirectory + L"\\removed.txt";
            const std::wstring nestedFile = nestedDirectory + L"\\nested.txt";
            const std::wstring retainedFile = retainedDirectory + L"\\retained.txt";
            const std::wstring excludedFile = nestedDirectory + L"\\excluded.bin";
            const std::string initialContent = "initial\n";
            for (const auto& file : {removedFile, nestedFile, retainedFile, excludedFile})
            {
                Assert::AreEqual(DWORD(ERROR_SUCCESS),
                    WriteToFile(file, initialContent.c_str(), initialContent.size()));
            }

            LogFileMonitor monitor(tempDirectory, L"*.txt", true, 0, L"json", L"");
            Sleep(WAIT_TIME_LOGFILEMONITOR_START);
            const std::string beforeMove = "tracked-before-directory-removal\n";
            Assert::AreEqual(DWORD(ERROR_SUCCESS), WriteToFile(removedFile, beforeMove.c_str(), beforeMove.size()));
            WaitForOutput(L"tracked-before-directory-removal");

            Assert::IsTrue(MoveFileW(removedDirectory.c_str(), movedDirectory.c_str()) != FALSE);
            const std::wstring markerFile = tempDirectory + L"\\marker.txt";
            const std::string marker = "directory-removal-processed\n";
            Assert::AreEqual(DWORD(ERROR_SUCCESS), WriteToFile(markerFile, marker.c_str(), marker.size()));
            WaitForOutput(L"directory-removal-processed");

            if (moveBack)
            {
                const std::string whileAway = "written-while-directory-was-away\n";
                Assert::AreEqual(DWORD(ERROR_SUCCESS),
                    WriteToFile(movedDirectory + L"\\removed.txt", whileAway.c_str(), whileAway.size()));
                Assert::IsTrue(MoveFileW(movedDirectory.c_str(), removedDirectory.c_str()) != FALSE);
                WaitForOutput(L"written-while-directory-was-away");

                const std::string returned = "appended-after-directory-returned\n";
                const std::string nested = "nested-append-after-directory-returned\n";
                const std::string retained = "similarly-named-sibling-still-monitored\n";
                Assert::AreEqual(DWORD(ERROR_SUCCESS), WriteToFile(removedFile, returned.c_str(), returned.size()));
                WaitForOutput(L"appended-after-directory-returned");
                Assert::AreEqual(DWORD(ERROR_SUCCESS), WriteToFile(nestedFile, nested.c_str(), nested.size()));
                WaitForOutput(L"nested-append-after-directory-returned");
                Assert::AreEqual(DWORD(ERROR_SUCCESS), WriteToFile(retainedFile, retained.c_str(), retained.size()));
                WaitForOutput(L"similarly-named-sibling-still-monitored");
            }

            StopMonitor(monitor);
            const size_t expectedCount = moveBack ? 4 : 2;
            Assert::AreEqual(expectedCount, monitor.m_logFilesInformation.size());
            Assert::AreEqual(expectedCount, monitor.m_longPaths.size());
            Assert::AreEqual(expectedCount, monitor.m_fileIds.size());
            Assert::IsTrue(monitor.m_logFilesInformation.find(L"RemovedDirectoryOther\\retained.txt") !=
                monitor.m_logFilesInformation.end());
            Assert::IsTrue(monitor.m_logFilesInformation.find(L"marker.txt") != monitor.m_logFilesInformation.end());
            for (const auto& entry : monitor.m_longPaths)
            {
                Assert::IsTrue(monitor.m_logFilesInformation.find(entry.second) != monitor.m_logFilesInformation.end());
            }
            for (const auto& entry : monitor.m_fileIds)
            {
                Assert::IsTrue(monitor.m_logFilesInformation.find(entry.second) != monitor.m_logFilesInformation.end());
            }
        }
    }

    TEST_METHOD(TestRemoveDirectoryHandlesShortPaths)
    {
        const std::wstring tempDirectory = CreateTempDirectory();
        Assert::IsFalse(tempDirectory.empty());
        directoriesToDeleteAtCleanup.push_back(tempDirectory);
        LogFileMonitor monitor(tempDirectory, L"*.txt", true, 0, L"json", L"");
        StopMonitor(monitor);

        const std::wstring removedFile = L"LongParentDirectory\\NestedDirectory\\removed.txt";
        const std::wstring retainedFile = L"LongParentDirectory\\NestedDirectoryOther\\retained.txt";
        const std::wstring otherFile = L"LongParentDirectoryOther\\retained.txt";
        for (const auto* path : {
            L"LongParentDirectory\\NestedDirectory",
            L"LONGPA~1\\NESTED~1",
            L"LongParentDirectory\\NESTED~1",
            L"longpa~1\\nesteddirectory",
            L"LongParentDirectory\\NESTED~1\\REMOVE~1.TXT"})
        {
            AddTrackedFile(monitor, removedFile, L"LONGPA~1\\NESTED~1\\REMOVE~1.TXT", 1);
            AddTrackedFile(monitor, retainedFile, L"LONGPA~1\\NESTED~2\\RETAIN~1.TXT", 2);
            AddTrackedFile(monitor, otherFile, L"LONGPA~2\\RETAIN~1.TXT", 3);

            DirChangeNotificationEvent event = {path, EventAction::Remove, GetTickCount64()};
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
            Assert::AreEqual(size_t(2), monitor.m_logFilesInformation.size(), path);
            Assert::IsTrue(monitor.m_logFilesInformation.find(retainedFile) != monitor.m_logFilesInformation.end());
            Assert::IsTrue(monitor.m_logFilesInformation.find(otherFile) != monitor.m_logFilesInformation.end());
            Assert::AreEqual(size_t(2), monitor.m_longPaths.size());
            Assert::AreEqual(size_t(2), monitor.m_fileIds.size());

            event.FileName = L"longpa~1";
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
            Assert::AreEqual(size_t(1), monitor.m_logFilesInformation.size());
            Assert::AreEqual(otherFile, monitor.m_longPaths.begin()->second);
            Assert::AreEqual(otherFile, monitor.m_fileIds.begin()->second);

            event.FileName = L"longparentdirectoryother";
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
            Assert::IsTrue(monitor.m_logFilesInformation.empty());
            Assert::IsTrue(monitor.m_longPaths.empty());
            Assert::IsTrue(monitor.m_fileIds.empty());
        }
    }

    TEST_METHOD(TestAddedDirectoryRespectsFilterAndRecursion)
    {
        const std::wstring tempDirectory = CreateTempDirectory();
        Assert::IsFalse(tempDirectory.empty());
        directoriesToDeleteAtCleanup.push_back(tempDirectory);
        for (bool includeSubfolders : {false, true})
        {
            const std::wstring watched = tempDirectory + (includeSubfolders ? L"\\recursive" : L"\\flat");
            Assert::IsTrue(CreateDirectoryW(watched.c_str(), nullptr) != FALSE);
            LogFileMonitor monitor(watched, L"matching.txt", includeSubfolders, 0, L"json", L"");
            StopMonitor(monitor);

            const std::wstring added = watched + L"\\AddedDirectory";
            Assert::IsTrue(CreateDirectoryW(added.c_str(), nullptr) != FALSE);
            const std::string content = "discovered-directory-content\n";
            for (const auto* file : {L"\\matching.txt", L"\\excluded.bin"})
            {
                Assert::AreEqual(DWORD(ERROR_SUCCESS),
                    WriteToFile(added + file, content.c_str(), content.size()));
            }
            DirChangeNotificationEvent event = {L"AddedDirectory", EventAction::Add, GetTickCount64()};
            Assert::AreEqual(DWORD(includeSubfolders ? ERROR_SUCCESS : ERROR_NO_MATCH),
                monitor.EnqueueDirChangeEvents(event, TRUE));
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileAddEventHandler(event));
            Assert::AreEqual(size_t(includeSubfolders ? 1 : 0), monitor.m_logFilesInformation.size());
            Assert::IsTrue(monitor.m_logFilesInformation.find(L"AddedDirectory\\excluded.bin") ==
                monitor.m_logFilesInformation.end());

            event.Action = EventAction::Remove;
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
            Assert::IsTrue(monitor.m_logFilesInformation.empty());
            event.Action = EventAction::RenameNew;
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRenameNewEventHandler(event));
            Assert::AreEqual(size_t(includeSubfolders ? 1 : 0), monitor.m_logFilesInformation.size());

            Assert::IsTrue(MoveFileW(added.c_str(), (watched + L"\\RenamedDirectory").c_str()) != FALSE);
            event.FileName = L"RenamedDirectory";
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRenameNewEventHandler(event));
            event.Action = EventAction::Remove;
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
            Assert::IsTrue(monitor.m_logFilesInformation.empty());
            Assert::IsTrue(monitor.m_longPaths.empty());
            Assert::IsTrue(monitor.m_fileIds.empty());

            DirChangeNotificationEvent reinit = {L"", EventAction::ReInit, GetTickCount64()};
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileReInitEventHandler(reinit));
            Assert::AreEqual(size_t(includeSubfolders ? 1 : 0), monitor.m_logFilesInformation.size());
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
            Assert::IsTrue(monitor.m_logFilesInformation.empty());
            Assert::IsTrue(monitor.m_longPaths.empty());
            Assert::IsTrue(monitor.m_fileIds.empty());
        }
    }

    TEST_METHOD(TestUntrackedRemovalLeavesTrackingUnchanged)
    {
        const std::wstring tempDirectory = CreateTempDirectory();
        Assert::IsFalse(tempDirectory.empty());
        directoriesToDeleteAtCleanup.push_back(tempDirectory);
        LogFileMonitor monitor(tempDirectory, L"*.txt", true, 0, L"json", L"");
        StopMonitor(monitor);
        for (DWORD id = 1; id <= 10000; ++id)
        {
            const std::wstring path = L"TrackedDirectory\\file" + std::to_wstring(id) + L".txt";
            AddTrackedFile(monitor, path, path, id);
        }
        DirChangeNotificationEvent event = {L"excluded.bin", EventAction::Remove, GetTickCount64()};
        LARGE_INTEGER frequency, start, finish;
        Assert::IsTrue(QueryPerformanceFrequency(&frequency) != FALSE);
        Assert::IsTrue(QueryPerformanceCounter(&start) != FALSE);
        for (int i = 0; i < 1000; ++i)
        {
            Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
        }
        Assert::IsTrue(QueryPerformanceCounter(&finish) != FALSE);
        const double elapsedMilliseconds = 1000.0 * (finish.QuadPart - start.QuadPart) / frequency.QuadPart;
        Logger::WriteMessage(Utility::FormatString(L"Untracked removal batch: %.3f ms", elapsedMilliseconds).c_str());
        Assert::AreEqual(size_t(10000), monitor.m_logFilesInformation.size());
        Assert::AreEqual(size_t(10000), monitor.m_longPaths.size());
        Assert::AreEqual(size_t(10000), monitor.m_fileIds.size());

        event.FileName = L"TRACKEDDIRECTORY\\FILE5000.TXT";
        Assert::AreEqual(DWORD(ERROR_SUCCESS), monitor.LogFileRemoveEventHandler(event));
        Assert::AreEqual(size_t(9999), monitor.m_logFilesInformation.size());
        Assert::AreEqual(size_t(9999), monitor.m_longPaths.size());
        Assert::AreEqual(size_t(9999), monitor.m_fileIds.size());
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

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
        Sleep(WAIT_TIME_LOGFILEMONITOR_START);

        //
        // Create the directory and wait to be recognized by the monitor.
        //
        BOOL directoryCreated = CreateDirectoryW(sourceFile.Directory.c_str(), NULL);
        Sleep(17000);

        Assert::IsTrue(directoryCreated != FALSE);

        directoriesToDeleteAtCleanup.push_back(sourceFile.Directory);

        //
        // Check that LogFileMonitor started successfully.
        //
        output = RecoverOuput();
        Assert::IsTrue(output.find(L"INFO") != std::wstring::npos);
        {
            std::wstring filename = sourceFile.Directory + L"\\testfile.txt";
            std::string content = "Hello World!";

            DWORD writeStatus = WriteToFile(filename, content.c_str(), content.length());
            Assert::AreEqual(DWORD{ERROR_SUCCESS}, writeStatus);

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            output = RecoverOuput();
            Assert::IsTrue(output.find((TO_WSTR(content)).c_str()) != std::wstring::npos);
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
        BOOL directoryCreated = CreateDirectoryW(subDirectory.c_str(), NULL);
        Assert::IsTrue(directoryCreated != FALSE);

        directoriesToDeleteAtCleanup.push_back(tempDirectory);

        //
        // Start the monitor
        //
        SourceFile sourceFile;
        sourceFile.Directory = tempDirectory;
        sourceFile.Filter = L"*.log";
        sourceFile.IncludeSubdirectories = true;
        sourceFile.WaitInSeconds = 10;

        fflush(stdout);
        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

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
            Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_LONG);

            output = RecoverOuput();
            Assert::AreEqual(L"", output.c_str());

            //
            // Check unmatched existing file is ignored.
            //
            WriteToFile(filename, content.c_str(), content.length());
            Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_LONG);

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
            Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_LONG);

            output = RecoverOuput();
            Assert::AreEqual(L"", output.c_str());

            //
            // Check unmatched existing file is ignored.
            //
            WriteToFile(filename, content.c_str(), content.length());
            Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_LONG);

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

        directoriesToDeleteAtCleanup.push_back(tempDirectory);

        //
        // Start the monitor
        //
        SourceFile sourceFile;
        sourceFile.Directory = tempDirectory;

        fflush(stdout);
        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(content) != std::wstring::npos);
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
                contentWithBom[i] =
                    (TCHAR)(((contentWithBom[i] << 8) & 0xFF00) + ((contentWithBom[i] >> 8) & 0xFF));
            }

            WriteToFile(filename, contentWithBom.c_str(), contentWithBom.length() * sizeof(WCHAR));

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(content) != std::wstring::npos);
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            std::wstring contentWide;
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), (int)content.length(), NULL, 0);
            contentWide.resize(size_needed);

            MultiByteToWideChar(
                CP_UTF8, 0, (LPCCH)content.c_str(), (int)content.length(),
                (LPWSTR)(contentWide.c_str()), size_needed);

            Assert::IsTrue(output.find(contentWide) != std::wstring::npos);
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

        directoriesToDeleteAtCleanup.push_back(tempDirectory);

        //
        // Create subdirectory
        //
        std::wstring subDirectory = tempDirectory + L"\\sub";
        BOOL directoryCreated = CreateDirectoryW(subDirectory.c_str(), NULL);
        Assert::IsTrue(directoryCreated != FALSE);

        //
        // Start the monitor
        //
        SourceFile sourceFile;
        sourceFile.Directory = tempDirectory;
        sourceFile.Filter = L"*.log";
        sourceFile.IncludeSubdirectories = true;
        sourceFile.WaitInSeconds = 10;

        fflush(stdout);
        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

        directoriesToDeleteAtCleanup.push_back(tempDirectory);

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
        BOOL directoryCreated = CreateDirectoryW(subDirectory.c_str(), NULL);
        Assert::IsTrue(directoryCreated != FALSE);

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
        sourceFile.WaitInSeconds = 10;

        fflush(stdout);
        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
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

            WriteToFile(oldFilename, content.c_str(), content.length());

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
        }

        //
        // Write to existing file and see stdout.
        //
        {
            fflush(stdout);
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

            std::string content = "Other new content";

            WriteToFile(oldFilenameSubdirectory, content.c_str(), content.length());

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
        }

        //
        // Write a long line (more than 4096 characters).
        //
        {
            fflush(stdout);
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

            std::wstring filename = sourceFile.Directory + L"\\longline.log";
            std::string content(20000, '#');

            WriteToFile(filename, content.c_str(), content.length());

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

        directoriesToDeleteAtCleanup.push_back(tempDirectory);

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
        BOOL directoryCreated = CreateDirectoryW(subDirectory.c_str(), NULL);
        Assert::IsTrue(directoryCreated != FALSE);

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
        sourceFile.WaitInSeconds = 10;

        fflush(stdout);
        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
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

            WriteToFile(newFilename, content.c_str(), content.length());

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);

            //
            // Rename it with a filter-unmatching name.
            //
            bool success = MoveFile(initialFilename.c_str(), renamedFilename.c_str());
            Assert::IsTrue(success);

            fflush(stdout);
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

            WriteToFile(renamedFilename, content.c_str(), content.length());
            Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_LONG);

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
            Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_LONG);

            Assert::AreEqual(L"", output.c_str());

            //
            // Rename it with a filter-matching name.
            //
            bool success = MoveFile(initialFilename.c_str(), renamedFilename.c_str());
            Assert::IsTrue(success);

            fflush(stdout);
            ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

            WriteToFile(renamedFilename, content.c_str(), content.length());

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            //
            // It should the FULL content of the file, that's why we need
            // to concatenate twice the content.
            //
            Assert::IsTrue(output.find((TO_WSTR(content) + TO_WSTR(content)).c_str()) != std::wstring::npos);
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

        directoriesToDeleteAtCleanup.push_back(tempDirectory);

        //
        // Create subdirectory.
        //
        std::string oldContent = "Old content";
        std::wstring subDirectory = tempDirectory + L"\\sub";
        BOOL directoryCreated = CreateDirectoryW(subDirectory.c_str(), NULL);
        Assert::IsTrue(directoryCreated != FALSE);

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
        sourceFile.WaitInSeconds = 10;

        fflush(stdout);
        ZeroMemory(bigOutBuf, sizeof(bigOutBuf));

        std::shared_ptr<LogFileMonitor> logfileMon = std::make_shared<LogFileMonitor>(
            sourceFile.Directory, sourceFile.Filter, sourceFile.IncludeSubdirectories,
            sourceFile.WaitInSeconds, L"json", L"");
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
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

            int retries = 0;
            do {
                retries++;
                Sleep(WAIT_TIME_LOGFILEMONITOR_AFTER_WRITE_SHORT);
                output = RecoverOuput();
            } while (output.empty() && retries < READ_OUTPUT_RETRIES);

            Assert::IsTrue(output.find(TO_WSTR(content)) != std::wstring::npos);
        }
    }
};
}  // namespace LogMonitorTests
