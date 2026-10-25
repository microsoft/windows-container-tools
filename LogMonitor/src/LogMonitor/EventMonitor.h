//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#include <vector>

#ifdef LOGMONITOR_TESTS
struct EventMonitorReplayTestHooks
{
    HANDLE batchReady;
    HANDLE releaseBatch;
    HANDLE shutdownRequested;
    LONG batchSize;
    LONG processedEvents;
    LONG closedEvents;
};
#endif

class EventMonitor final
{
 public:
    EventMonitor() = delete;

    EventMonitor(
        _In_ const std::vector<EventLogChannel>& eventChannels,
        _In_ bool EventFormatMultiLine,
        _In_ bool StartAtOldestRecord,
        _In_ std::wstring LogFormat,
        _In_ std::wstring CustomLogFormat
#ifdef LOGMONITOR_TESTS
        , _In_opt_ EventMonitorReplayTestHooks* ReplayTestHooks = nullptr
#endif
        );

    ~EventMonitor();

    static std::wstring EventFieldsMapping(_In_ std::wstring eventField, _In_ void* pLogEntryData);

 private:
    static constexpr int EVENT_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS = 5 * 1000;
    static constexpr DWORD EVENT_READ_TIMEOUT_MILLIS = 100;
    static constexpr int EVENT_ARRAY_SIZE = 10;

    const std::vector<EventLogChannel> m_eventChannels;
    bool m_eventFormatMultiLine;
    bool m_startAtOldestRecord;
    std::wstring m_logFormat;
    std::wstring m_customLogFormat;

    struct EventLogEntry {
        std::wstring source;
        std::wstring eventSource;
        std::wstring eventTime;
        std::wstring eventChannel;
        std::wstring eventLevel;
        UINT16 eventId;
        std::wstring eventMessage;
    };

    //
    // Signaled by destructor to request the spawned thread to stop.
    //
    HANDLE m_stopEvent;

    //
    // Handle to an event subscriber thread.
    //
    HANDLE m_eventMonitorThread;

    std::vector<wchar_t> m_eventMessageBuffer;

#ifdef LOGMONITOR_TESTS
    EventMonitorReplayTestHooks* m_replayTestHooks;
#endif

    DWORD StartEventMonitor();

    static DWORD WINAPI StartEventMonitorStatic(
        _In_ LPVOID Context
        );

    DWORD CheckForStop() const;

    std::wstring ConstructWindowsEventQuery(
        _In_ const std::vector<EventLogChannel>& EventChannels
        );

    DWORD EnumerateResults(
        _In_ EVT_HANDLE ResultsHandle
        );

    DWORD PrintEvent(
        _In_ const HANDLE& EventHandle
        );


    DWORD EnableEventLogChannels();

    static DWORD EnableEventLogChannel(
        _In_ LPCWSTR ChannelPath);
};
