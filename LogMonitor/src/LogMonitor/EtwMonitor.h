//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

typedef LPTSTR(NTAPI* PIPV6ADDRTOSTRING)(
    const IN6_ADDR* Addr,
    LPTSTR S
    );

class EtwMonitor final
{
public:

    EtwMonitor() = delete;

    EtwMonitor(
        _In_ const std::vector<ETWProvider>& Providers,
        _In_ bool EventFormatMultiLine
    );

    ~EtwMonitor();

private:

    static constexpr int ETW_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS = 5 * 1000;

    std::vector<ETWProvider> m_providersConfig;
    bool m_eventFormatMultiLine;
    TRACEHANDLE  m_startTraceHandle;

    //
    // Vectors used to store an EVENT_TRACE_PROPERTIES object.
    //
    std::vector<BYTE>    m_vecEventTracePropsBuffer;
    std::vector<BYTE>    m_vecStopTracePropsBuffer;

    //
    // Handle to an event subscriber thread.
    //    
    HANDLE m_ETWMonitorThread;

    DWORD PointerSize;

    DWORD StartEtwMonitor();

    static DWORD FilterValidProviders(
        _In_ const std::vector<ETWProvider>& Providers,
        _Out_ std::vector<ETWProvider>& ValidProviders
    );

    static void WINAPI OnEventRecordTramp(
        _In_ PEVENT_RECORD EventRecord
    );

    static DWORD StartEtwMonitorStatic(
        _In_ LPVOID Context
    );

    DWORD StartTraceSession(
        _Out_ TRACEHANDLE& TraceSessionHandle
    );

    DWORD OnRecordEvent(
        _In_ const PEVENT_RECORD EventRecord
    );

    DWORD PrintEvent(
        _In_ const PEVENT_RECORD EventRecord,
        _In_ const PTRACE_EVENT_INFO EventInfo
    );

    DWORD FormatMetadata(
        _In_ const PEVENT_RECORD EventRecord,
        _In_ const PTRACE_EVENT_INFO EventInfo,
        _Inout_ std::wstring& Result
    );

    //
    // Event data functions
    //
    DWORD FormatData(
        _In_ const PEVENT_RECORD EventRecord,
        _In_ const PTRACE_EVENT_INFO EventInfo,
        _Inout_ std::wstring& Result
    );

    DWORD _FormatData(
        _In_ const PEVENT_RECORD EventRecord,
        _In_ const PTRACE_EVENT_INFO EventInfo,
        _In_ USHORT Index,
        _Inout_ PBYTE& UserData,
        _In_ PBYTE EndOfUserData,
        _Inout_ std::wostringstream& Result
    );

    DWORD GetPropertyLength(
        _In_ const PEVENT_RECORD EventRecord,
        _In_ const PTRACE_EVENT_INFO EventInfo,
        _In_ const USHORT Index,
        _Out_ USHORT& PropertyLength
    );

    DWORD GetArraySize(
        _In_ const PEVENT_RECORD EventRecord,
        _In_ const PTRACE_EVENT_INFO EventInfo,
        _In_ const USHORT Index,
        _Out_ USHORT& ArraySize
    );

    DWORD GetMapInfo(
        _In_ const PEVENT_RECORD EventRecord,
        _In_ LPWSTR MapName,
        _In_ DWORD DecodingSource,
        _Out_ PEVENT_MAP_INFO& MapInfo
    );

    inline void RemoveTrailingSpace(
        _In_ PEVENT_MAP_INFO MapInfo
    );
};
