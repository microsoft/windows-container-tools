//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "LogFileMonitor.h"
#include "shlwapi.h"
#include "LogWriter.h"
#include "Utility.h"

using namespace std;

///
/// LogFileMonitor.cpp
/// 
/// Monitors a log directory for changes to the log files matching the criteria specified by a filter.
/// 
/// LogFileMonitor starts a monitor thread that waits for file change notifications from ReadDirectoryChangesW
/// or for a stop event to be set. It also starts a worker thread that processes the change notification events.
/// 
/// The destructor signals the stop event and waits up to LOG_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS for the monitoring 
/// thread to exit. To prevent the thread from out-living LogFileMonitor, the destructor fails fast
/// if the wait fails or times out. This also ensures the callback is not being called and will not be 
/// called once LogFileMonitor is destroyed.
///


#define LOG_DIR_NOTIFY_FILTERS   (FILE_NOTIFY_CHANGE_CREATION |         \
                                  FILE_NOTIFY_CHANGE_DIR_NAME |         \
                                  FILE_NOTIFY_CHANGE_FILE_NAME |        \
                                  FILE_NOTIFY_CHANGE_LAST_WRITE |       \
                                  FILE_NOTIFY_CHANGE_SIZE)

///
/// Constructor creates a thread to monitor log directory changes and waits until
/// that thread registers for directory change notifications. This ensures that no
/// changes to log files are missed once the LogFileMonitor object is created.
///
/// \param LogDirectory:        The log directory to be monitored
/// \param Filter:              The filter to apply when looking fr log files
/// \param IncludeSubfolders:   TRUE if subdirectories also needs to be monitored
///
LogFileMonitor::LogFileMonitor(_In_ const std::wstring& LogDirectory,
                               _In_ const std::wstring& Filter,
                               _In_ bool IncludeSubfolders
                               ) :
                               m_logDirectory(LogDirectory),
                               m_filter(Filter),
                               m_includeSubfolders(IncludeSubfolders)
{
    m_stopEvent = NULL;
    m_overlappedEvent = NULL;
    m_workerThreadEvent = NULL;
    m_logFilesChangeHandlerThread = NULL;
    m_dirMonitorStartedEvent = NULL;
    m_logDirMonitorThread = NULL;
    m_logDirHandle = INVALID_HANDLE_VALUE;

    InitializeSRWLock(&m_eventQueueLock);

    while (!m_logDirectory.empty() && m_logDirectory[ m_logDirectory.size() - 1 ] == L'\\')
    {
        m_logDirectory.resize(m_logDirectory.size() - 1);
    }
    m_logDirectory = Utility::GetLongPath(PREFIX_EXTENDED_PATH + m_logDirectory);
    m_shortLogDirectory = Utility::GetShortPath(m_logDirectory);

    if (m_filter.empty())
    {
        m_filter = L"*";
    }

    m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if(!m_stopEvent)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    m_overlappedEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);
    if(!m_overlappedEvent)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    m_overlapped.hEvent = m_overlappedEvent;

    m_workerThreadEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);
    if(!m_workerThreadEvent)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    m_dirMonitorStartedEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if(!m_dirMonitorStartedEvent)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    m_readLogFilesFromStart = false;

    m_logDirMonitorThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)&LogFileMonitor::StartLogFileMonitorStatic, this, 0, nullptr);
    if(!m_logDirMonitorThread)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateThread");
    }

    //
    // Wait until log directory change notification is registered or the monitoring thread exited.
    //
    const DWORD eventsCount = 2;
    HANDLE events[eventsCount] = {m_dirMonitorStartedEvent, m_logDirMonitorThread};
    DWORD waitResult = WaitForMultipleObjects(eventsCount, events, FALSE, INFINITE);

    if (waitResult != WAIT_OBJECT_0)
    {
        if (waitResult == WAIT_OBJECT_0 + 1)
        {
            DWORD threadExitCode;
            HRESULT hr = GetExitCodeThread(m_logDirMonitorThread, &threadExitCode)
                            ? HRESULT_FROM_WIN32(threadExitCode) : HRESULT_FROM_WIN32(GetLastError());

            if (FAILED(hr))
            {
                throw std::system_error(std::error_code(hr, std::system_category()), "StartLogFileMonitorStatic");
            }
        }
        else
        {
            throw std::system_error(std::error_code(GetLastError(), std::system_category()), "WaitForMultipleObjects");
        }
    }
}


LogFileMonitor::~LogFileMonitor()
{
    const DWORD eventsCount = 2;
    HANDLE events[eventsCount] = {m_logDirMonitorThread, m_logFilesChangeHandlerThread};

    if(!SetEvent(m_stopEvent))
    {
        logWriter.TraceError(
            Utility::FormatString(L"SetEvent failed with %lu", GetLastError()).c_str()
        );
    }
    else
    {
        //
        // Wait for directory monitor and change notification handler threads to exit.
        //
        DWORD waitResult = WaitForMultipleObjects(eventsCount, events, TRUE, LOG_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS);

        if (waitResult != WAIT_OBJECT_0)
        {
            HRESULT hr = (waitResult == WAIT_FAILED) ? HRESULT_FROM_WIN32(GetLastError())
                                                           : HRESULT_FROM_WIN32(waitResult);
            if (FAILED(hr))
            {
                logWriter.TraceError(
                    Utility::FormatString(L"Failed to wait for log file monitor to stop. Log directory: %s Error: %lu", m_logDirectory.c_str(), hr).c_str()
                );
            }
        }
    }

    if (!m_logDirMonitorThread)
    {
        CloseHandle(m_logDirMonitorThread);
    }

    if (!m_logFilesChangeHandlerThread)
    {
        CloseHandle(m_logFilesChangeHandlerThread);
    }

    if (!m_dirMonitorStartedEvent)
    {
        CloseHandle(m_dirMonitorStartedEvent);
    }

    if (!m_workerThreadEvent)
    {
        CloseHandle(m_workerThreadEvent);
    }

    if (!m_overlappedEvent)
    {
        CloseHandle(m_overlappedEvent);
    }

    if (!m_stopEvent)
    {
        CloseHandle(m_stopEvent);
    }

    if (m_logDirHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_logDirHandle);
    }

}


///
/// Worker routine to monitor log directory changes.
///
/// \param Context: LogFileMonitor object passed in as callback context
///
/// \return ERROR_SUCCESS: If log monitoring is started successfully
///         Failure:       If log directory monitoring failed.
///
DWORD
LogFileMonitor::StartLogFileMonitorStatic(
    _In_ LPVOID Context
    )
{
    auto pThis = reinterpret_cast<LogFileMonitor*>(Context);
    try
    {
        DWORD status = pThis->StartLogFileMonitor();
        if (status != ERROR_SUCCESS)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to start log file monitor. Log files in a directory %s will not be monitored. Error: %lu", pThis->m_logDirectory.c_str(), status).c_str()
            );
        }
        return status;
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start log file monitor. Log files in a directory %s will not be monitored. %S", pThis->m_logDirectory.c_str(), ex.what()).c_str()
        );
        return E_FAIL;
    }
    catch (...)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start log file monitor. Log files in a directory %s will not be monitored.", pThis->m_logDirectory.c_str()).c_str()
        );
        return E_FAIL;
    }
}


///
/// Entry for the spawned log file monitor thread. It loops to wait for either the stop event in which case it exits 
/// or for the log file changes. When log file changes are notified, it inserts the changed file information into a 
/// queue, signal the change notification worker thread, resets, and starts the wait again.
///
/// \return ERROR_SUCCESS: If log monitoring is started successfully
///         Failure:       If log directory monitoring failed.
///
DWORD
LogFileMonitor::StartLogFileMonitor()
{
    DWORD status = ERROR_SUCCESS;
    const DWORD eventsCount = 2;
    bool changeHandlerThreadCreated = false;
    bool dirMonitorStartedEventSignalled = false;

    //
    // Order stop event first so that stop is prioritized if both events are already signalled (changes 
    // are available but stop has been called).
    //
    HANDLE events[eventsCount] = {m_stopEvent, m_overlapped.hEvent};
    bool stopWatching = false;

    m_logDirHandle = CreateFileW (m_logDirectory.c_str(),
                                 FILE_LIST_DIRECTORY,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr,
                                 OPEN_EXISTING,
                                 FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                 nullptr);

    if (m_logDirHandle == INVALID_HANDLE_VALUE)
    {
        status = GetLastError();
    }

    if (status == ERROR_FILE_NOT_FOUND ||
        status == ERROR_PATH_NOT_FOUND)
    {
        //
        // Log directory is not created yet. Keep retrying every
        // 5 seconds for upto one minute. Also start reading the
        // log files from the begining, instead of current end of
        // file.
        //
        const DWORD maxRetryCount = 12;
        DWORD retryCount = 1;
        LARGE_INTEGER liDueTime;
        INT64 millisecondsToWait = 5000LL;
        liDueTime.QuadPart = -millisecondsToWait*10000LL; // wait time in 100 nanoseconds

        m_readLogFilesFromStart = true;

        SetEvent(m_dirMonitorStartedEvent);
        dirMonitorStartedEventSignalled = true;

        HANDLE timerEvent = CreateWaitableTimer(NULL, FALSE, NULL);
        if (!timerEvent)
        {
            status = GetLastError();
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to create timer object. Log directory %ws will not be monitored for log entries. Error=%d",
                    m_logDirectory.c_str(),
                    status
                ).c_str()
            );
            return status;
        }
        
        HANDLE dirOpenEvents[eventsCount] = {m_stopEvent, timerEvent};

        while (status == ERROR_FILE_NOT_FOUND &&
                retryCount < maxRetryCount)
        {
			logWriter.WriteConsoleLog(L"It's getting here 1");
            if (!SetWaitableTimer(timerEvent, &liDueTime, 0, NULL, NULL, 0))
            {
                status = GetLastError();
                
                logWriter.TraceError(
                    Utility::FormatString(L"Failed to set timer object to monitor log file changes in directory %s. Error: %lu", m_logDirectory.c_str(), status).c_str()
                );
                break;
            }
			logWriter.WriteConsoleLog(L"It's getting here 2");
            DWORD wait = WaitForMultipleObjects(eventsCount, dirOpenEvents, FALSE, INFINITE);
            switch(wait)
            {
                case WAIT_OBJECT_0:
                {
                    //
                    // The process is exiting. Stop the timer and return.
                    //
                    CancelWaitableTimer(timerEvent);
                    CloseHandle(timerEvent);
                    return status;
                }
            
                case WAIT_OBJECT_0 + 1:
                {
                    //
                    // Timer event. Retry opening directory handle.
                    //
                    break;
                }
            
                default:
                {
                    //
                    //Wait failed, return the failure.
                    //
                    status = GetLastError();

                    CancelWaitableTimer(timerEvent);
                    CloseHandle(timerEvent);

                    return status;
                }
                    
            }
			logWriter.WriteConsoleLog(L"It's getting here 3");
            m_logDirHandle = CreateFileW (m_logDirectory.c_str(),
                                          FILE_LIST_DIRECTORY,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          nullptr,
                                          OPEN_EXISTING,
                                          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                          nullptr);

            if (m_logDirHandle == INVALID_HANDLE_VALUE)
            {
				logWriter.WriteConsoleLog(L"It's getting here 4");
                status = GetLastError();
                ++retryCount;
            }
            else
            {
				logWriter.WriteConsoleLog(L"It's getting here 5");
                status = ERROR_SUCCESS;
                break;
            }
        }

        CancelWaitableTimer(timerEvent);
        CloseHandle(timerEvent);
    }

    if (status != ERROR_SUCCESS)
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"Failed to open log directory handle. Directory: %ws Error=%d",
                m_logDirectory.c_str(),
                status
            ).c_str()
        );
        return status;
    }

    status = InitializeDirectoryChangeEventsQueue();
    if (status != ERROR_SUCCESS)
    {
        //
        // Failed to enumerate log files in a directory.
        // Ignore the failure and continue. Some log file
        // information will not be monitored.
        //
        logWriter.TraceError(
            Utility::FormatString(
                L"Failed to open enumerate log directory. Directory: %ws Error=%d",
                m_logDirectory.c_str(),
                status
            ).c_str()
        );
    }

    //
    // Initialize the list of log file in a given directory and start a worker thread to
    // process directory change notification events.
    //
    while (!stopWatching)
    {
        std::fill(records.begin(), records.end(), (BYTE)0); // Reset previous entries if any.
        m_overlapped.Offset = 0;
        m_overlapped.OffsetHigh = 0;
        BOOL success = ReadDirectoryChangesW(m_logDirHandle,
                                             records.data(),
                                             static_cast<ULONG>(records.size()),
                                             true,
                                             LOG_DIR_NOTIFY_FILTERS,
                                             nullptr,
                                             &m_overlapped,
                                             nullptr);

        if (!success) 
        {
            status = GetLastError();
            if (status == ERROR_NOTIFY_ENUM_DIR) 
            {
                status = ERROR_SUCCESS;
                if (!dirMonitorStartedEventSignalled)
                {
                    SetEvent(m_dirMonitorStartedEvent);
                    dirMonitorStartedEventSignalled = true;
                }

                //
                // The change buffer was full and all events were discarded. Scan the directory again
                // and create file add event for each log file. In cse of failure, just log message
                // and continue.
                //
                DirChangeNotificationEvent changeEvent;

                changeEvent.Timestamp = GetTickCount64();
                changeEvent.Action = EventAction::ReInit;

                AcquireSRWLockExclusive(&m_eventQueueLock);

                m_directoryChangeEvents.emplace(changeEvent);

                if (m_directoryChangeEvents.size() == 1)
                {
                    //wprintf(L"Signalling worker thread to start processing the event queue\n");
                    if(!SetEvent(m_workerThreadEvent))
                    {
                        logWriter.TraceError(
                            Utility::FormatString(
                                L"Failed to signal worker thread of log changes. This may cause lag in log file monitoring or lost log messages. Log directory: %ws, Error: %d",
                                m_logDirectory.c_str(),
                                GetLastError()
                            ).c_str()
                        );
                    }
                }

                ReleaseSRWLockExclusive(&m_eventQueueLock);

                continue;
            } 
            else 
            {
                logWriter.TraceError(
                    Utility::FormatString(
                        L"Failed to monitor log directory changes. Log directory: %ws, Error: %d",
                        m_logDirectory.c_str(),
                        status
                    ).c_str()
                );
            }
        }

        if (!dirMonitorStartedEventSignalled)
        {
            SetEvent(m_dirMonitorStartedEvent);
            dirMonitorStartedEventSignalled = true;
        }

        if (status == ERROR_SUCCESS)
        {
            if (!changeHandlerThreadCreated)
            {
                m_logFilesChangeHandlerThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)&LogFileMonitor::LogFilesChangeHandlerStatic, this, 0, nullptr);
                if (!m_logFilesChangeHandlerThread)
                {
                    status = GetLastError();
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Failed to create a thread to read log files from directory: %ws Error=%d",
                            m_logDirectory.c_str(),
                            status
                        ).c_str()
                    );

                    return status;
                }
                else
                {
                    changeHandlerThreadCreated = true;
                }
            }

            DWORD wait = WaitForMultipleObjects(eventsCount, events, FALSE, INFINITE);
            switch(wait)
            {
                case WAIT_OBJECT_0:
                {
                    //
                    // Clear the event queue
                    //
                    AcquireSRWLockExclusive(&m_eventQueueLock);
                    
                    while (m_directoryChangeEvents.size() > 0)
                    {
                        m_directoryChangeEvents.pop();
                    }

                    ReleaseSRWLockExclusive(&m_eventQueueLock);

                    stopWatching = true;
                }
                break;

                case WAIT_OBJECT_0 + 1:
                {
                    status = LogDirectoryChangeNotificationHandler();
                }
                break;

                default:
                    status = GetLastError();
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Failed to monitor log directory changes. Wait operation failed. Log directory: %ws, Error: %d",
                            m_logDirectory.c_str(),
                            status
                        ).c_str()
                    );
                    stopWatching = true;
            }
        }
        else
        {
            stopWatching = true;
        }
    }

    return status;

}


///
/// Check if the filename matches a filter.
///
/// \param FileName         The filename to be evaluated.
/// \param SearchPattern    The filter to evaluate with.
///
/// \return True if the file matches filter. Otherwise, false
///
inline BOOL
LogFileMonitor::FileMatchesFilter(
    _In_ LPCWSTR FileName,
    _In_ LPCWSTR SearchPattern
    )
{
    return PathMatchSpec(FileName, SearchPattern);
}


DWORD
LogFileMonitor::InitializeMonitoredFilesInfo()
{
    DWORD status = ERROR_SUCCESS;

    return status;
}


DWORD
LogFileMonitor::LogDirectoryChangeNotificationHandler()
{
    DWORD status = ERROR_SUCCESS;
    DWORD dwBytesTransfered = 0;
    DWORD dwNextEntryOffset = 0;

    if (!GetOverlappedResult(m_logDirHandle, &m_overlapped, &dwBytesTransfered, FALSE))
    {
        status = GetLastError();
        return status;
    }

    if (dwBytesTransfered)
    {
        int i = 0;
        FILE_NOTIFY_INFORMATION *fileNotificationInfo = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(records.data());
        WCHAR pszFileName[4096];
        do
        {
            i++;

            //
            // Extract the FileName
            //
            DWORD dwFileNameLength = fileNotificationInfo->FileNameLength / sizeof(WCHAR);
            if (dwFileNameLength > 4096)
            {
                dwFileNameLength = 4095;
            }
            CopyMemory(pszFileName, fileNotificationInfo->FileName, sizeof(WCHAR)*dwFileNameLength);
            pszFileName[dwFileNameLength] = '\0';

            std::wstring fileName = pszFileName;

            DirChangeNotificationEvent changeEvent;

            switch (fileNotificationInfo->Action)
            {
                case FILE_ACTION_ADDED:
                {
                    //wprintf(L"\t[%d] === DirectoryListenThread: <%ws> ADDED ===\n", i, fileName.c_str());
                    changeEvent.Action = EventAction::Add;
                    break;
                }
                case FILE_ACTION_REMOVED:
                {
                    //wprintf(L"\t[%d] === DirectoryListenThread: <%ws> REMOVED ===\n", i, fileName.c_str());
                    changeEvent.Action = EventAction::Remove;
                    break;
                }
                case (FILE_ACTION_MODIFIED):
                {
                    //wprintf(L"\t[%d] === DirectoryListenThread: <%ws> MODIFIED ===\n", i, fileName.c_str());
                    changeEvent.Action = EventAction::Modify;
                    break;
                }
                case (FILE_ACTION_RENAMED_OLD_NAME):
                {
                    //wprintf(L"\t[%d] === DirectoryListenThread: <%ws> RENAMED_OLD_NAME ===\n", i, fileName.c_str());
                    changeEvent.Action = EventAction::RenameOld;
                    break;
                }
                case (FILE_ACTION_RENAMED_NEW_NAME):
                {
                    //wprintf(L"\t[%d] === DirectoryListenThread: <%ws> RENAMED_NEW_NAME ===\n", i, fileName.c_str());
                    changeEvent.Action = EventAction::RenameNew;
                    break;
                }
                default:
                {
                    //wprintf(L"\t[%d] === DirectoryListenThread: <%ws> UnknownAction = 0x%x ===\n", i, fileName.c_str(), fileNotificationInfo->Action);
                    changeEvent.Action = EventAction::Unknown;
                    break;
                }
            }

            if (changeEvent.Action == EventAction::Add ||
                changeEvent.Action == EventAction::Remove ||
                changeEvent.Action == EventAction::Modify ||
                changeEvent.Action == EventAction::RenameOld ||
                changeEvent.Action == EventAction::RenameNew)
            {
                changeEvent.FileName = fileName;
                changeEvent.Timestamp = GetTickCount64();

                AcquireSRWLockExclusive(&m_eventQueueLock);

                m_directoryChangeEvents.emplace(changeEvent);

                if (m_directoryChangeEvents.size() == 1)
                {
                    //wprintf(L"Signalling worker thread to start processing the event queue\n");
                    if(!SetEvent(m_workerThreadEvent))
                    {
                        ReleaseSRWLockExclusive(&m_eventQueueLock);
                        return GetLastError();
                    }
                }

                ReleaseSRWLockExclusive(&m_eventQueueLock);
            }
            
            dwNextEntryOffset = fileNotificationInfo->NextEntryOffset;
            fileNotificationInfo = (FILE_NOTIFY_INFORMATION*)((LPBYTE)fileNotificationInfo + fileNotificationInfo->NextEntryOffset);

        } while (dwNextEntryOffset);
    }
    else
    {
        logWriter.TraceError(L"DirectoryListenThread: ERROR - GetOverlappedResult returned 1 bytes transferred");
    }

    return S_OK;
}


DWORD
LogFileMonitor::InitializeDirectoryChangeEventsQueue()
{
    DWORD status = ERROR_SUCCESS;
    std::vector<std::pair<std::wstring, FILE_ID_INFO> > logFiles;

    //wprintf(L"InitializeDirectoryChangeEventsQueue\n");

    status = GetFilesInDirectory(m_logDirectory, m_filter, logFiles, m_includeSubfolders);

    if (status == ERROR_SUCCESS)
    {
        AcquireSRWLockExclusive(&m_eventQueueLock);

        //
        // Read log files from the start only when the tool is launched
        // for the first time and the log files did not exist before.
        // Subsequent attempts to enumerate the directory should set
        // the file next read offset to the size of the file.
        //
        bool readLogFileFromStart = m_readLogFilesFromStart;

        m_readLogFilesFromStart = false;

        for (auto file: logFiles)
        {
            const std::wstring& fileName = file.first;
            const FILE_ID_INFO& fileId = file.second;

            const std::wstring longPath = fileName.substr(m_logDirectory.size() + 1);

            auto element = GetLogFilesInformationIt(longPath);
            
            if (element != m_logFilesInformation.end())
            {
                //
                // Log file already exist. Do nothing.
                //
            }
            else
            {
                auto logFileInfo = std::make_shared<LogFileInformation>();

                const std::wstring shortPath = Utility::GetShortPath(fileName).substr(m_shortLogDirectory.size() + 1);

                logFileInfo->FileName = longPath;
                logFileInfo->NextReadOffset = 0;
                logFileInfo->LastReadTimestamp = 0;

                if (!readLogFileFromStart)
                {
                    LARGE_INTEGER fileSize = {};

                    HANDLE logFile = CreateFileW(fileName.c_str(),
                                                  GENERIC_READ,
                                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                  nullptr,
                                                  OPEN_EXISTING,
                                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                                  nullptr);
                    if (logFile == INVALID_HANDLE_VALUE)
                    {
                        logWriter.TraceError(
                            Utility::FormatString(
                                L"ReadLogFile: Failed to open file %ws. Error = %d",
                                fileName.c_str(),
                                GetLastError()
                            ).c_str()
                        );

                        //
                        // Ignore failure and continue. In the worst case we will
                        // read the entire log file.
                        //
                        continue;
                    }

                    if (!GetFileSizeEx (logFile, &fileSize))
                    {
                        logWriter.TraceError(
                            Utility::FormatString(
                                L"ReadLogFile: Failed to get size of file %ws. Error = %d",
                                fileName.c_str(),
                                GetLastError()
                            ).c_str()
                        );
                    }
                    else 
                    {
                        logFileInfo->NextReadOffset = fileSize.QuadPart;
                    }

                    CloseHandle(logFile);
                }

                m_longPaths[shortPath] = longPath;
                m_logFilesInformation[longPath] = std::move(logFileInfo);
                m_fileIds[fileId] = longPath;
            }

            DirChangeNotificationEvent changeEvent;

            changeEvent.Action = EventAction::Modify;
            changeEvent.FileName = longPath;
            changeEvent.Timestamp = GetTickCount64();
            
            m_directoryChangeEvents.emplace(changeEvent);

            if (m_directoryChangeEvents.size() == 1)
            {
                //wprintf(L"Signalling worker thread to start processing the event queue\n");
                if(!SetEvent(m_workerThreadEvent))
                {
                    ReleaseSRWLockExclusive(&m_eventQueueLock);

                    logWriter.TraceError(
                        Utility::FormatString(L"Failed to signal worker thread to process log file changes. Error=%d\n", GetLastError()).c_str()
                    );
                    return GetLastError();
                }
            }
        }

        ReleaseSRWLockExclusive(&m_eventQueueLock);
    }
    else
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"Failed to enumerate log directory %ws. Error=%d",
                m_logDirectory.c_str(),
                GetLastError()
            ).c_str()
        );
    }

    return status;
}


DWORD
LogFileMonitor::LogFilesChangeHandlerStatic(
    _In_ LPVOID Context
    )
{
    auto pThis = reinterpret_cast<LogFileMonitor*>(Context);
    try
    {
        DWORD status = pThis->LogFilesChangeHandler();
        if (status != ERROR_SUCCESS)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to monitor log directory changes. Log files in a directory %s will not be monitored. Error: %lu", pThis->m_logDirectory.c_str(), status).c_str()
            );
        }
        return status;
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to monitor log directory changes. Log files in a directory %s will not be monitored. %S", pThis->m_logDirectory.c_str(), ex.what()).c_str()
        );
        return E_FAIL;
    }
    catch (...)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to monitor log directory changes. Log files in a directory %s will not be monitored", pThis->m_logDirectory.c_str()).c_str()
        );
        return E_FAIL;
    }
}


DWORD
LogFileMonitor::LogFilesChangeHandler()
{
    DWORD status = ERROR_SUCCESS;
    bool stopWatching = false;
    const DWORD eventsCount = 3;

    LARGE_INTEGER liDueTime;
    INT64 millisecondsToWait = 30000LL;
    liDueTime.QuadPart = -millisecondsToWait*10000LL; // wait time in 100 nanoseconds

    HANDLE timerEvent = CreateWaitableTimer(NULL, FALSE, NULL);
    if (!timerEvent)
    {
        status = GetLastError();
        logWriter.TraceError(
            Utility::FormatString(L"Failed to create timer object to monitor log file changes in directory %s. Error: %lu", m_logDirectory.c_str(), status).c_str()
        );
        return status;
    }

    HANDLE events[eventsCount] = {m_stopEvent, m_workerThreadEvent, timerEvent};

    status = InitializeDirectoryChangeEventsQueue();

    if (status != ERROR_SUCCESS)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to enumerate log directory %ws. Some log files may not be monitored for application logs", m_logDirectory.c_str()).c_str()
        );
    }

    if (!SetWaitableTimer(timerEvent, &liDueTime, 0, NULL, NULL, 0))
    {
        status = GetLastError();
        
        logWriter.TraceError(
            Utility::FormatString(L"Failed to set timer object to monitor log file changes in directory %s. Error: %lu", m_logDirectory.c_str(), status).c_str()
        );
    }

    while (!stopWatching)
    {
        DWORD wait = WaitForMultipleObjects(eventsCount, events, FALSE, INFINITE);
        switch(wait)
        {
            case WAIT_OBJECT_0:
            {
                stopWatching = true;
                CancelWaitableTimer(timerEvent);
            }
            break;

            case WAIT_OBJECT_0 + 1:
            {
                AcquireSRWLockExclusive(&m_eventQueueLock);

                while (m_directoryChangeEvents.size() > 0)
                {
                    auto event = m_directoryChangeEvents.front();
                    m_directoryChangeEvents.pop();

                    //
                    // Try to recover the long path. The worst case is when it's already
                    // a long path, and it will make a useless variable reassign. 
                    //
                    auto it = m_longPaths.find(event.FileName);

                    if (it != m_longPaths.end())
                    {
                        event.FileName = it->second;
                    }

                    ReleaseSRWLockExclusive(&m_eventQueueLock);
                    
                    switch (event.Action)
                    {
                        case EventAction::Add:
                        {
                            if (FileMatchesFilter((LPCWSTR)(event.FileName.c_str()), m_filter.c_str()))
                            {
                                status = LogFileAddEventHandler(event);
                            }
                            else
                            {
                                //
                                // It could be because the name was short formatted. Make it long path and try again.
                                //
                                event.FileName = Utility::GetLongPath(m_logDirectory + L'\\' + event.FileName)
                                                    .substr(m_logDirectory.size() + 1);

                                if (FileMatchesFilter((LPCWSTR)(event.FileName.c_str()), m_filter.c_str()))
                                {
                                    status = LogFileAddEventHandler(event);
                                }
                            }
                            break;
                        }

                        case EventAction::Modify:
                        {
                            if (FileMatchesFilter((LPCWSTR)(event.FileName.c_str()), m_filter.c_str()))
                            {
                                status = LogFileModifyEventHandler(event);
                            }
                            break;
                        }

                        case EventAction::Remove:
                        {
                            if (FileMatchesFilter((LPCWSTR)(event.FileName.c_str()), m_filter.c_str()))
                            {
                                status = LogFileRemoveEventHandler(event);
                            }
                            break;
                        }

                        case EventAction::RenameOld:
                        {
                            //
                            // Nothing to do
                            //
                            break;
                        }

                        case EventAction::RenameNew:
                        {
                            status = LogFileRenameNewEventHandler(event);
                            break;
                        }

                        case EventAction::ReInit:
                        {
                            status = LogFileReInitEventHandler(event);
                            break;
                        }

                        default:
                            break;
                    }
                    
                    AcquireSRWLockExclusive(&m_eventQueueLock);
                }

                if(!ResetEvent(m_workerThreadEvent))
                {
                    status = GetLastError();

                    logWriter.TraceError(
                        Utility::FormatString(L"Failed to reset event object to monitor log file changes in directory %s. Error: %lu", m_logDirectory.c_str(), status).c_str()
                    );
                    stopWatching = true;
                }

                if (!SetWaitableTimer(timerEvent, &liDueTime, 0, NULL, NULL, 0))
                {
                    status = GetLastError();

                    logWriter.TraceError(
                        Utility::FormatString(L"Failed to set timer object to monitor log file changes in directory %s. Error: %lu", m_logDirectory.c_str(), status).c_str()
                    );
                }

                ReleaseSRWLockExclusive(&m_eventQueueLock);
            }
            break;

            case WAIT_OBJECT_0 + 2:
            {
                map<std::wstring, std::shared_ptr<LogFileInformation>>::iterator it;

                //wprintf(L"LogFilesChangeHandler: Timer event\n");

                for ( it = m_logFilesInformation.begin(); it != m_logFilesInformation.end(); it++ )
                {
                    ReadLogFile(it->second);
                }

                if (!SetWaitableTimer(timerEvent, &liDueTime, 0, NULL, NULL, 0))
                {
                    status = GetLastError();

                    logWriter.TraceError(
                        Utility::FormatString(L"Failed to set timer object to monitor log file changes in directory %s. Error: %lu", m_logDirectory.c_str(), status).c_str()
                    );
                }
            }
            break;

            default:
            {
                status = GetLastError();
                
                logWriter.TraceError(
                    Utility::FormatString(L"Failed to wait on directory change notification events to monitor log file changes in directory %s. Error: %lu", m_logDirectory.c_str(), status).c_str()
                );
                stopWatching = true;
            }
            break;
        }
    }

    return status;
}


DWORD
LogFileMonitor::LogFileAddEventHandler(
    _In_ DirChangeNotificationEvent& Event
    )
{
    DWORD status = ERROR_SUCCESS;

    auto element = GetLogFilesInformationIt(Event.FileName);

    if (element != m_logFilesInformation.end())
    {
        //
        // Log file already exist. Do nothing.
        //
    }
    else 
    {
        const std::wstring fullLongPath = Utility::GetLongPath(m_logDirectory + L'\\' + Event.FileName);

        DWORD ftyp = GetFileAttributesW(fullLongPath.c_str());
        if (ftyp == INVALID_FILE_ATTRIBUTES)
        {
            status = GetLastError();
            logWriter.TraceError(
                Utility::FormatString(
                    L"GetFileAttributesW: Failed to get info of file %ws. Error = %d",
                    fullLongPath.c_str(),
                    status
                ).c_str()
            );

            return status;
        }

        if (!(ftyp & FILE_ATTRIBUTE_DIRECTORY))
        {
            auto logFileInfo = std::make_shared<LogFileInformation>();

            //
            // Get the short and long relative path
            //
            const std::wstring longPath = fullLongPath.substr(m_logDirectory.size() + 1);

            const std::wstring shortPath = Utility::GetShortPath(fullLongPath).substr(m_shortLogDirectory.size() + 1);

            logFileInfo->FileName = longPath;
            logFileInfo->NextReadOffset = 0;
            logFileInfo->LastReadTimestamp = 0;
            logFileInfo->EncodingType = LM_FILETYPE::FileTypeUnknown;

            FILE_ID_INFO fileId{ 0 };
            status = GetFileId(fullLongPath, fileId);

            if (status != ERROR_SUCCESS)
            {
                status = GetLastError();
                logWriter.TraceError(
                    Utility::FormatString(
                        L"GetFileId: Failed to get info of file %ws. Error = %d",
                        fullLongPath.c_str(),
                        status
                    ).c_str()
                );
            }

            status = ReadLogFile(logFileInfo);

            m_fileIds[fileId] = longPath;
            m_longPaths[shortPath] = longPath;
            m_logFilesInformation[longPath] = std::move(logFileInfo);
        }
    }

    return status;
}


DWORD
LogFileMonitor::LogFileRemoveEventHandler(
    _In_ DirChangeNotificationEvent& Event
    )
{
    DWORD status = ERROR_SUCCESS;
    bool isShortPath = false;

    auto element = GetLogFilesInformationIt(Event.FileName, &isShortPath);

    if (element != m_logFilesInformation.end())
    {
        std::wstring longPath = element->second->FileName;

        m_logFilesInformation.erase(element);

        auto longPathIterator = (!isShortPath) ? m_longPaths.find(Event.FileName) :
            std::find_if(
                m_longPaths.begin(),
                m_longPaths.end(),
                [&longPath](const auto& it) { return _wcsicmp(it.second.c_str(), longPath.c_str()) == 0; });

        if (longPathIterator != m_longPaths.end())
        {
            m_longPaths.erase(longPathIterator);
        }
        
        auto fileIdIterator =
            std::find_if(
                m_fileIds.begin(),
                m_fileIds.end(),
                [&longPath](const auto& it) { return _wcsicmp(it.second.c_str(), longPath.c_str()) == 0; });

        if (fileIdIterator != m_fileIds.end())
        {
            m_fileIds.erase(fileIdIterator);
        }
    }
    else
    {
        //
        // Log file not found.
        //
    }

    return status;

}


DWORD
LogFileMonitor::LogFileModifyEventHandler(
    _In_ DirChangeNotificationEvent& Event
    )
{
    DWORD status = ERROR_SUCCESS;

    auto element = GetLogFilesInformationIt(Event.FileName);

    if (element != m_logFilesInformation.end() &&
        Event.Timestamp > element->second->LastReadTimestamp)
    {
        status = ReadLogFile(element->second);
    }
    else
    {
        //
        // Log file not found or event is too old.
        //
    }

    return status;
}

DWORD
LogFileMonitor::LogFileRenameNewEventHandler(
    _In_ DirChangeNotificationEvent& Event
    )
{
    DWORD status = ERROR_SUCCESS;

    const std::wstring fullLongPath = Utility::GetLongPath(m_logDirectory + L'\\' + Event.FileName);
    const std::wstring longPath = fullLongPath.substr(m_logDirectory.size() + 1);

    DWORD ftyp = GetFileAttributesW(fullLongPath.c_str());
    if (ftyp == INVALID_FILE_ATTRIBUTES)
    {
        status = GetLastError();
        logWriter.TraceError(
            Utility::FormatString(
                L"GetFileAttributesW: Failed to get info of file %ws. Error = %d",
                fullLongPath.c_str(),
                status
            ).c_str()
        );

        return status;
    }

    if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (m_includeSubfolders)
        {
            std::vector<std::pair<std::wstring, FILE_ID_INFO>> logFiles;

            GetFilesInDirectory(fullLongPath, m_filter, logFiles, m_includeSubfolders);

            for (auto file : logFiles)  
            {
                const std::wstring& fileName = file.first;
                const FILE_ID_INFO& fileId = file.second;

                auto itFileId = m_fileIds.find(fileId);

                if (itFileId != m_fileIds.end())
                {
                    RenameFileInMaps(fileName, itFileId->second, fileId);
                }
            }
        }
    }
    else 
    {
        FILE_ID_INFO fileId{ 0 };
        GetFileId(fullLongPath, fileId);

        auto it = m_fileIds.find(fileId);

        if (it != m_fileIds.end())
        {
            if (FileMatchesFilter(fullLongPath.c_str(), m_filter.c_str()))
            {
                RenameFileInMaps(fullLongPath, it->second, fileId);
            }
            else
            {
                DirChangeNotificationEvent e = Event;

                e.FileName = it->second;
                e.Action = EventAction::Remove;

                LogFileRemoveEventHandler(e);
            }
        }
        else if (FileMatchesFilter(fullLongPath.c_str(), m_filter.c_str()))
        {
            DirChangeNotificationEvent e = Event;

            e.FileName = longPath;
            e.Action = EventAction::Add;

            LogFileAddEventHandler(e);
        }
    }

    return status;
}

///
/// This functions replace all the ocurrence of the OldName in the maps
/// with the new name, based on a FileId. If it doesn't find the old
/// elements in the maps, creates new ones.
///
/// \param NewFullName      The new full filename.
/// \param OldName          The old relative filename.
/// \param FileId           FileId of the file.
///
void
LogFileMonitor::RenameFileInMaps(
    _In_ const std::wstring& NewFullName,
    _In_ const std::wstring& OldName,
    _In_ const FILE_ID_INFO& FileId
)
{
    const std::wstring longPath = NewFullName.substr(m_logDirectory.size() + 1);
    const std::wstring shortPath = Utility::GetShortPath(NewFullName).substr(m_shortLogDirectory.size() + 1);

    auto it = GetLogFilesInformationIt(OldName);

    shared_ptr<LogFileInformation> fileInfo;
    if (it != m_logFilesInformation.end())
    {
        fileInfo = it->second;
        fileInfo->FileName = longPath;
        m_logFilesInformation.erase(it);
    }
    else
    {
        fileInfo = std::make_shared<LogFileInformation>();
        fileInfo->FileName = longPath;
        fileInfo->EncodingType = LM_FILETYPE::FileTypeUnknown;
        fileInfo->LastReadTimestamp = 0;
        fileInfo->NextReadOffset = 0;
    }

    //
    // Try first the case when the long and short path are the same.
    //
    auto longPathIterator = m_longPaths.find(OldName);
    if (longPathIterator == m_longPaths.end())
    {
        //
        // Look for the map element with value equal to long path.
        //
        longPathIterator = 
            std::find_if(
                m_longPaths.begin(),
                m_longPaths.end(),
                [&OldName](const auto& it) { return _wcsicmp(it.second.c_str(), OldName.c_str()) == 0; });

    }

    if (longPathIterator != m_longPaths.end())
    {
        m_longPaths.erase(longPathIterator);
    }

    m_logFilesInformation[longPath] = fileInfo;
    m_longPaths[shortPath] = longPath;
    m_fileIds[FileId] = longPath;
}


DWORD
LogFileMonitor::LogFileReInitEventHandler(
    _In_ DirChangeNotificationEvent& Event
    )
{
    DWORD status = ERROR_SUCCESS;
    std::vector<std::pair<std::wstring, FILE_ID_INFO>> logFiles;

    status = GetFilesInDirectory(m_logDirectory, m_filter, logFiles, m_includeSubfolders);

    if (status == ERROR_SUCCESS)
    {
        for (auto file: logFiles)
        {
            const std::wstring& fileName = file.first;
            const FILE_ID_INFO& fileId = file.second;

            const std::wstring longPath = fileName.substr(m_logDirectory.size() + 1);

            auto element = GetLogFilesInformationIt(longPath);

            if (element != m_logFilesInformation.end())
            {
                //
                // Log file already exist. Do nothing.
                //
            }
            else
            {
                auto logFileInfo = std::make_shared<LogFileInformation>();

                const std::wstring shortPath = Utility::GetShortPath(fileName).substr(m_shortLogDirectory.size() + 1);

                logFileInfo->FileName = longPath;
                logFileInfo->NextReadOffset = 0;
                logFileInfo->LastReadTimestamp = 0;

                m_longPaths[shortPath] = longPath;
                m_logFilesInformation[longPath] = std::move(logFileInfo);
                m_fileIds[fileId] = longPath;
            }

        }
    }
    else
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"Failed to enumerate log directory %ws. Error=%d",
                m_logDirectory.c_str(),
                GetLastError()
            ).c_str()
        );
    }

    map<std::wstring, std::shared_ptr<LogFileInformation>>::iterator it;

    //wprintf(L"LogFilesChangeHandler: Timer event\n");

    for ( it = m_logFilesInformation.begin(); it != m_logFilesInformation.end(); it++ )
    {
        ReadLogFile(it->second);
    }

    return status;
}


DWORD
LogFileMonitor::ReadLogFile(
    _Inout_ std::shared_ptr<LogFileInformation> LogFileInfo
    )
{
    DWORD status = ERROR_SUCCESS;
    OVERLAPPED overlapped = { 0, 0, 0, 0, nullptr };
    BYTE bom[3 * sizeof(char)] = { 0, 0, 0 }; // Bom could be up to 3 bytes size in UTF8.
    bool wasBomRead = false;

    //wprintf(L"ReadLogFile: Reading file %ws, offset=%I64d\n", LogFileInfo->FileName.c_str(), LogFileInfo->NextReadOffset);
    const std::wstring fullLongPath = m_logDirectory + L'\\' + LogFileInfo->FileName;

    HANDLE logFile = CreateFileW(fullLongPath.c_str(),
                                  GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                  nullptr);
    if (logFile == INVALID_HANDLE_VALUE)
    {
        status = GetLastError();
        if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
        {
            //
            // This errors should not trace anything.
            //
            status = ERROR_SUCCESS;
        }
        else
        {
            logWriter.TraceError(
                Utility::FormatString(
                    L"ReadLogFile: Failed to open file %ws. Error = %d",
                    fullLongPath.c_str(),
                    status
                ).c_str()
            );
        }
        return status;
    }

    DWORD dwPtr = SetFilePointer(logFile, 0L, NULL, FILE_BEGIN);
    if (dwPtr == INVALID_SET_FILE_POINTER)
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"ReadLogFile: SetFilePointer to open file %ws. Error = %d",
                fullLongPath.c_str(),
                GetLastError()
            ).c_str()
        );
    }

    //
    // If the beginning of the file hasn't been read yet, don't get the BOM.
    // Also, if EncodingType is already known, skip this.
    //
    if (LogFileInfo->NextReadOffset >= 3 && LogFileInfo->EncodingType == LM_FILETYPE::FileTypeUnknown)
    {
        DWORD bytesRead = 0;
        overlapped.Offset = 0;
        overlapped.OffsetHigh = 0;

        if (!::ReadFile(
                logFile,
                &bom,
                sizeof(bom),
                &bytesRead,
                &overlapped)
            || bytesRead < (sizeof(bom) - 1)) // UTF16 BOM could be only 2 bytes
        {
            //
            // It wasn't possible to read the BOM or it was smaller than expected.
            //
            wasBomRead = false;
        }
        else
        {
            wasBomRead = true;
        }
    }

    const DWORD bytesToRead = 4096;
    std::vector<BYTE> logFileContents(
        static_cast<size_t>(bytesToRead), 0);
    DWORD bytesRead = 0;

    std::wstring currentLineBuffer;

    //wprintf(L"ReadLogFile: File = %ws. bytesToRead = %d\n", LogFileInfo->FileName.c_str(), bytesToRead);

    //
    // It's important to catch a posible error inside the loop, to at least print
    // the content of currentLineBuffer, if it has any.
    //
    try
    {
        do
        {
            bytesRead = 0;

            overlapped.Offset = UINT(LogFileInfo->NextReadOffset & 0xFFFFFFFF);
            overlapped.OffsetHigh = UINT(LogFileInfo->NextReadOffset >> 32);

            LogFileInfo->LastReadTimestamp = GetTickCount64();

            if (!::ReadFile(
                logFile,
                logFileContents.data(),
                bytesToRead,
                &bytesRead,
                &overlapped))
            {
                status = GetLastError();
                if (status == ERROR_HANDLE_EOF)
                {
                    bytesRead = 0;
                    //wprintf(L"ReadLogFile:End of file. File = %ws. Error = %d\n", LogFileInfo->FileName.c_str(), status);
                    status = ERROR_SUCCESS;
                }
                else
                {
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"ReadLogFile: File read error. File = %ws. Error = %d",
                            LogFileInfo->FileName.c_str(),
                            GetLastError()
                        ).c_str()
                    );

                    LogFileInfo->LastReadTimestamp = 0;
                }
                break;
            }

            if (bytesRead > 0)
            {
                //
                // Get file type if it's still unknown
                //
                UINT foundBomSize = 0;
                if (LogFileInfo->EncodingType == LM_FILETYPE::FileTypeUnknown)
                {
                    if (wasBomRead)
                    {
                        LogFileInfo->EncodingType = this->FileTypeFromBuffer(logFileContents.data(), bytesRead, bom, sizeof(bom), foundBomSize);
                    }
                    else
                    {
                        LogFileInfo->EncodingType = this->FileTypeFromBuffer(logFileContents.data(), bytesRead, logFileContents.data(), bytesRead, foundBomSize);
                    }
                }

                //
                // Check if we need to skip only some bytes, instead of the whole BOM
                //
                if (foundBomSize > LogFileInfo->NextReadOffset)
                {
                    foundBomSize = (UINT)(foundBomSize - LogFileInfo->NextReadOffset);
                }
                else
                {
                    //
                    // It means that the BOM is not in the read content
                    //
                    foundBomSize = 0;
                }

                //
                // Decode read string to UTF16, skipping the BOM if necessary.
                //
                const std::wstring decodedString = ConvertStringToUTF16(logFileContents.data() + foundBomSize, bytesRead - foundBomSize, LogFileInfo->EncodingType);

                //
                // Search 'new line' characters, and if found, print the line.
                //
                std::size_t found = decodedString.find_last_of(L"\n\r");
                std::size_t remainingStringIndex = 0;

                if (found != std::string::npos)
                {
                    remainingStringIndex = found + 1;

                    if ((found - 1) >= 0
                        && (decodedString[(found - 1)] == L'\n' || decodedString[(found - 1)] == L'\r')
                        && decodedString[(found - 1)] != decodedString[found])
                    {
                        //
                        // Is a CR LF or LF CR new line, and we don't want to print two new lines
                        //
                        found--;
                    }
                    // TODO: handle the writing to file case. The UTF-16 BOM should be added at the beginning, and use WriteLog function.
                    // Also, in the writing to file scenario, instead of print each line here, we should store the lines to print, and then,
                    // print them once time.

                    //
                    // Write to console log.
                    //
                    if (!currentLineBuffer.empty())
                    {
                        try
                        {
                            currentLineBuffer.insert(currentLineBuffer.end(), decodedString.begin(), decodedString.begin() + found);
                            logWriter.WriteConsoleLog(currentLineBuffer);
                        }
                        catch (...)
                        {
                            //
                            // If insert failed, print them now.
                            //
                            logWriter.WriteConsoleLog(currentLineBuffer);

                            std::wstring remainingBuffer = decodedString.substr(0, found);
                            logWriter.WriteConsoleLog(remainingBuffer);
                        }

                        currentLineBuffer.clear();
                    }
                    else
                    {
                        //
                        // newLineBuffer was empty, so only print the found line.
                        //
                        logWriter.WriteConsoleLog(decodedString.substr(0, found));
                    }
                }
                //
                // If the whole line wasn't printed (the last character wasn't a new line), add the remainder
                // to currentLineBuffer.
                //
                if(remainingStringIndex < decodedString.size())
                {
                    try
                    {
                        currentLineBuffer.insert(currentLineBuffer.end(),
                                                 decodedString.begin() + remainingStringIndex,
                                                 decodedString.end());
                    }
                    catch (...)
                    {
                        //
                        // If insert failed, print buffers now and clear currentLineBuffer.
                        //
                        std::wstring remainingBuffer(decodedString.begin() + remainingStringIndex, decodedString.end());

                        logWriter.WriteConsoleLog(currentLineBuffer);
                        logWriter.WriteConsoleLog(remainingBuffer);
                        currentLineBuffer.clear();
                    }
                }
            }

            //wprintf(L"Log file %ws read succeeded.\n", LogFileInfo->FileName.c_str());    

            LogFileInfo->NextReadOffset += bytesRead; //fileSize.QuadPart;
        } while (bytesRead > 0);
    }
    catch (...) {}

    if (!currentLineBuffer.empty())
    {
        //
        // If we reach EOF, print the last line.
        //
        logWriter.WriteConsoleLog(currentLineBuffer);
    }

    CloseHandle(logFile);

    return status;
}


DWORD
LogFileMonitor::GetFilesInDirectory(
    _In_ const std::wstring& FolderPath,
    _In_ const std::wstring& SearchPattern,
    _Out_ std::vector<std::pair<std::wstring, FILE_ID_INFO>>& Files,
    _In_ bool ShouldLookInSubfolders)
{
    WIN32_FIND_DATA ffd;
    DWORD status = ERROR_SUCCESS;

    //
    // First search through subfolders.
    //
    if (ShouldLookInSubfolders)
    {
        std::wstring toSearch = FolderPath + L"\\*";
        HANDLE hFind = FindFirstFileW(toSearch.c_str(), &ffd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (ffd.cFileName[0] != L'.'))
                {
                    std::wstring subfolderPath = FolderPath + L"\\" + ffd.cFileName;
                    status = GetFilesInDirectory(subfolderPath, SearchPattern, Files, true);
                }
            } while (FindNextFile(hFind, &ffd) != 0 && (status == ERROR_SUCCESS));
            FindClose(hFind);
        }
        else
        {
            status = GetLastError();
        }
    }

    if (status == ERROR_SUCCESS)
    {
        //
        // Now search for files using SearchPattern
        //
        std::wstring toSearch = FolderPath + L"\\" + SearchPattern;
        HANDLE hFind = FindFirstFileW(toSearch.c_str(), &ffd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                //
                // Filter out the files that do not match the specified filter.
                //
                if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && FileMatchesFilter(ffd.cFileName, SearchPattern.c_str()))
                { 
                    std::wstring fileName = FolderPath + L"\\" + ffd.cFileName;

                    FILE_ID_INFO fileId{ 0 };
                    GetFileId(fileName, fileId);

                    //wprintf(L"Started monitoring file: %s\n",fileName.c_str());

                    Files.push_back({ fileName, fileId });
                }
            } while (FindNextFile(hFind, &ffd) != 0);

            status = GetLastError();
            
            if (status == ERROR_NO_MORE_FILES)
            {
                status = ERROR_SUCCESS;
            }

            FindClose(hFind);
        }
    }


    return status;
}

LM_FILETYPE
LogFileMonitor::FileTypeFromBuffer(
    _In_reads_bytes_(ContentSize) LPBYTE FileContents,
    _In_ UINT ContentSize,
    _In_reads_bytes_(BomSize) LPBYTE Bom,
    _In_ UINT BomSize,
    _Out_ UINT& FoundBomSize
)
{
    LM_FILETYPE lmFileType = LM_FILETYPE::FileTypeUnknown;
    FoundBomSize = 0;

    PWSTR szBuf = (PWSTR)FileContents;
    if (ContentSize <= 1 && BomSize <= 1)
    {
        return lmFileType;
    }

    switch (*((PWSTR)Bom))
    {
    //
    // look for the standard BOMs.
    //
    case BYTE_ORDER_MARK:
        lmFileType = LM_FILETYPE::UTF16LE;
        FoundBomSize = sizeof(WCHAR);
        break;

    case REVERSE_BYTE_ORDER_MARK:
        lmFileType = LM_FILETYPE::UTF16BE;
        FoundBomSize = sizeof(WCHAR);
        break;

    //
    // UTF bom has 3 bytes.
    //
    case BOM_UTF8_HALF:
        if (BomSize > 2 && ((BYTE) * ((Bom)+2) == BOM_UTF8_2HALF))
        {
            lmFileType = LM_FILETYPE::UTF8;
            FoundBomSize = sizeof(char) * 3;
            break;
        }

        //
        // If UTF8 BOM doesn't match, continue with the evaluation
        //
    default:
        //
        // Is the file unicode without BOM ?
        //
        if (Utility::IsInputTextUnicode((LPCSTR)FileContents, ContentSize))
        {
            lmFileType = LM_FILETYPE::UTF16LE;
        }
        else
        {
            //
            // Is the file UTF-8 even though it doesn't have UTF-8 BOM ?
            //
            if (Utility::IsTextUTF8((LPCSTR)FileContents, ContentSize))
            {
                lmFileType = LM_FILETYPE::UTF8;
            }
            //
            // well, it is most likely an ansi file.
            //
            else
            {
                lmFileType = LM_FILETYPE::ANSI;
            }
        }
        break;
    }

    return lmFileType;
}


std::wstring
LogFileMonitor::ConvertStringToUTF16(
    _In_reads_bytes_(StringSize) LPBYTE StringPtr,
    _In_ UINT StringSize,
    _In_ LM_FILETYPE EncodingType
)
{
    std::wstring Result;
    if (StringSize == 0)
    {
        return std::wstring();
    }

    switch (EncodingType)
    {
    case LM_FILETYPE::UTF16LE:
    {
        Result = wstring((wchar_t*)StringPtr, (wchar_t*)(StringPtr + StringSize));
        break;
    }
    case LM_FILETYPE::UTF16BE:
    {
        Result = wstring((wchar_t*)StringPtr, (wchar_t*)(StringPtr + StringSize));

        //
        // Reverse each wide character, to make it little endian
        //
        for (unsigned int i = 0; i < Result.size(); i++)
        {
            Result[i] = (TCHAR)(((Result[i] << 8) & 0xFF00) + ((Result[i] >> 8) & 0xFF));
        }
        break;
    }
    case LM_FILETYPE::UTF8:
    {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)StringPtr, StringSize, NULL, 0);
        Result.resize(size_needed);

        MultiByteToWideChar(CP_UTF8, 0, (LPCCH)StringPtr, StringSize, (LPWSTR)(Result.data()), size_needed);
        break;
    }
    default:
    {
        std::string tempStr((char*)StringPtr, (char*)(StringPtr + StringSize));
        //
        // ANSI
        //
        Result = wstring(tempStr.begin(), tempStr.end());
    }
    }

    return Result;
}

///
/// Gets an iterator, using first the Key as if it was a long path, and if it
/// isn't found recovering the long path using Key as a short path.
///
/// \param Key         The filename to look for.
/// \param IsShortPath Optional boolean pointer. If it's passed, it will return
///     false if the key wasn't a short path. Otherwise, will be true.
///
/// \return Iterator to a m_logFilesInformation element.
///
LogFileMonitor::LogFileInfoMap::iterator
LogFileMonitor::GetLogFilesInformationIt(
    _In_ const std::wstring& Key,
    _Out_opt_ bool* IsShortPath
)
{
    auto element = m_logFilesInformation.find(Key);

    if (element == m_logFilesInformation.end())
    {
        auto longPath = m_longPaths.find(Key);
        if (longPath != m_longPaths.end())
        {
            element = m_logFilesInformation.find(longPath->second);
        }

        if (IsShortPath != NULL)
        {
            *IsShortPath = true;
        }
    }
    else
    {
        if (IsShortPath != NULL)
        {
            *IsShortPath = false;
        }
    }

    return element;
}

///
/// Gets the file id from a specified file.
///
/// \param FullLongPath     The filename of the file to get its id.
/// \param FileId           An output parameter, with the file id.
/// \param Handle           Optional. If it is passed, it doesn't need to
///     open the file again.
///
/// \return DWORD with a standard result of the function.
///
DWORD
LogFileMonitor::GetFileId(
    _In_ const std::wstring& FullLongPath,
    _Out_ FILE_ID_INFO& FileId,
    _In_opt_ HANDLE Handle
)
{
    DWORD status = ERROR_SUCCESS;

    //
    // Flag to close the handle at the end of the function.
    //
    bool handleWasGiven = false;

    if (Handle == INVALID_HANDLE_VALUE)
    {
        Handle = CreateFileW(FullLongPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);
    }
    else
    {
        handleWasGiven = true;
    }

    if (Handle == INVALID_HANDLE_VALUE)
    {
        status = GetLastError();
        logWriter.TraceError(
            Utility::FormatString(
                L"CreateFileW: Failed to open file %ws. Error = %d",
                FullLongPath.c_str(),
                status
            ).c_str()
        );
        return status;
    }

    BOOL infoQueryStatus = ::GetFileInformationByHandleEx(
        Handle,
        FileIdInfo,
        &FileId,
        sizeof(FILE_ID_INFO));

    if (!infoQueryStatus)
    {
        status = GetLastError();
        logWriter.TraceError(
            Utility::FormatString(
                L"GetFileInformationByHandleEx: Failed to get info of file %ws. Error = %d",
                FullLongPath.c_str(),
                status
            ).c_str()
        );
    }

    if (!handleWasGiven)
    {
        CloseHandle(Handle);
    }

    return status;
}
