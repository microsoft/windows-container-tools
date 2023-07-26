//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#define REVERSE_BYTE_ORDER_MARK 0xFFFE
#define BYTE_ORDER_MARK         0xFEFF

#define BOM_UTF8_HALF           0xBBEF
#define BOM_UTF8_2HALF          0xBF

#define PREFIX_EXTENDED_PATH L"\\\\?\\"

///
/// LogMonitor filetype
///
enum LM_FILETYPE {
    FileTypeUnknown,
    ANSI,
    UTF16LE,
    UTF16BE,
    UTF8
};

struct LogFileInformation
{
    std::wstring FileName;
    UINT64 NextReadOffset;
    UINT64 LastReadTimestamp;
    LM_FILETYPE EncodingType;
};

enum class EventAction
{
    Add = 0,
    Modify = 1,
    Remove = 2,
    RenameOld = 3,
    RenameNew = 4,
    ReInit = 5,
    Unknown = 5,
};


struct DirChangeNotificationEvent
{
    std::wstring FileName;
    EventAction Action;
    UINT64 Timestamp;
};


class LogFileMonitor final
{
public:
    LogFileMonitor() = delete;

    LogFileMonitor(
        _In_ const std::wstring& LogDirectory,
        _In_ const std::wstring& Filter,
        _In_ bool IncludeSubfolders,
        _In_ const std::double_t& WaitInSeconds
        );

    ~LogFileMonitor();

private:
    static constexpr int LOG_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS = 5 * 1000;
    static constexpr int RECORDS_BUFFER_SIZE_BYTES = 8 * 1024;

    std::wstring m_logDirectory;
    std::wstring m_shortLogDirectory;
    std::wstring m_filter;
    std::double_t m_waitInSeconds;
    bool m_includeSubfolders;

    //
    // Signaled by destructor to request the spawned thread to stop.
    //
    HANDLE m_stopEvent;

    HANDLE m_dirMonitorStartedEvent;

    HANDLE m_workerThreadEvent;

    HANDLE m_logDirHandle;

    OVERLAPPED m_overlapped;
    HANDLE m_overlappedEvent;

    //
    // Must be DWORD aligned so allocated on the heap.
    //
    std::vector<BYTE> records = std::vector<BYTE>(RECORDS_BUFFER_SIZE_BYTES);

    //
    // Handle to an event subscriber thread.
    //
    HANDLE m_logDirMonitorThread;

    //
    // Handle to an change notification event handler thread.
    //
    HANDLE m_logFilesChangeHandlerThread;

    SRWLOCK m_eventQueueLock;

    //
    // Case insensitive comparison
    //
    struct ci_less
    {
        bool operator() (const std::wstring & s1, const std::wstring & s2) const
        {
            return _wcsicmp(s1.c_str(), s2.c_str()) < 0;
        }
    };

    typedef std::map<std::wstring, std::shared_ptr<LogFileInformation>, ci_less> LogFileInfoMap;

    LogFileInfoMap m_logFilesInformation;

    std::map<std::wstring, std::wstring, ci_less> m_longPaths;

    //
    // FILE_ID_INFO comparison
    //
    struct file_id_less
    {
        bool operator() (const FILE_ID_INFO& id1, const FILE_ID_INFO& id2) const
        {
            return id1.VolumeSerialNumber < id2.VolumeSerialNumber ||
                (id1.VolumeSerialNumber == id2.VolumeSerialNumber
                    && memcmp(id1.FileId.Identifier, id2.FileId.Identifier, sizeof(id1.FileId.Identifier)) < 0);
        }
    };

    std::map<FILE_ID_INFO, std::wstring, file_id_less> m_fileIds;

    std::queue<DirChangeNotificationEvent> m_directoryChangeEvents;

    bool m_readLogFilesFromStart;

    DWORD EnqueueDirChangeEvents(DirChangeNotificationEvent event, BOOLEAN lock);

    DWORD StartLogFileMonitor();

    static DWORD StartLogFileMonitorStatic(
        _In_ LPVOID Context
        );

    static inline BOOL FileMatchesFilter(
        _In_ LPCWSTR FileName,
        _In_ LPCWSTR SearchPattern
        );

    DWORD InitializeMonitoredFilesInfo();

    DWORD LogDirectoryChangeNotificationHandler();

    static DWORD LogFilesChangeHandlerStatic(
        _In_ LPVOID Context
        );

    DWORD LogFilesChangeHandler();

    DWORD InitializeDirectoryChangeEventsQueue();

    static DWORD GetFilesInDirectory(
        _In_ const std::wstring& FolderPath,
        _In_ const std::wstring& SearchPattern,
        _Out_ std::vector<std::pair<std::wstring, FILE_ID_INFO>>& Files,
        _In_ bool ShouldLookInSubfolders
        );

    DWORD LogFileAddEventHandler(DirChangeNotificationEvent& Event);

    DWORD LogFileRemoveEventHandler(DirChangeNotificationEvent& Event);

    DWORD LogFileModifyEventHandler(DirChangeNotificationEvent& Event);

    DWORD LogFileRenameNewEventHandler(DirChangeNotificationEvent& Event);

    void RenameFileInMaps(
        _In_ const std::wstring& NewFullName,
        _In_ const std::wstring& OldName,
        _In_ const FILE_ID_INFO& FileId
        );

    DWORD LogFileReInitEventHandler(DirChangeNotificationEvent& Event);

    DWORD ReadLogFile(
        _Inout_ std::shared_ptr<LogFileInformation> LogFileInfo
        );

    void WriteToConsole(
        _In_ std::wstring Message,
        _In_ std::wstring FileName
    );

    LM_FILETYPE FileTypeFromBuffer(
        _In_reads_bytes_(ContentSize) LPBYTE FileContents,
        _In_ UINT ContentSize,
        _In_reads_bytes_(BomSize) LPBYTE Bom,
        _In_ UINT BomSize,
        _Out_ UINT& FoundBomSize
        );

    std::wstring ConvertStringToUTF16(
        _In_reads_bytes_(StringSize) LPBYTE StringPtr,
        _In_ UINT StringSize,
        _In_ LM_FILETYPE EncodingType
        );

    LogFileInfoMap::iterator GetLogFilesInformationIt(
        _In_ const std::wstring& Key,
        _Out_opt_ bool* IsShortPath = NULL
        );

    static DWORD GetFileId(
        _In_ const std::wstring& FullLongPath,
        _Out_ FILE_ID_INFO& FileId,
        _In_opt_ HANDLE Handle = INVALID_HANDLE_VALUE
        );
};
