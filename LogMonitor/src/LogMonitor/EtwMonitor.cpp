//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

#define MAX_NAME 256

using namespace std;

///
/// EtwMonitor.cpp
///
/// Monitors an ETW event.
///
/// EtwMonitor starts a monitor thread that waits for events from real-time
/// providers, with a specified name or GUID. The same thread processes the
/// changes and invokes the callback.
///
/// The code is based on example from:
///  Using TdhFormatProperty to Consume Event Data
///  https://docs.microsoft.com/en-us/windows/win32/etw/using-tdhformatproperty-to-consume-event-data
///

//
// Session tracing name that LogMonitor will use.
//
static const std::wstring g_sessionName = L"Log Monitor ETW Session";

EtwMonitor::EtwMonitor(
    _In_ const std::vector<ETWProvider>& Providers,
    _In_ bool EventFormatMultiLine
    ) :
    m_eventFormatMultiLine(EventFormatMultiLine)
{
    //
    // This is set as 'true' to stop processing events.
    //
    m_stopFlag = false;

    FilterValidProviders(Providers, m_providersConfig);

    if (m_providersConfig.empty())
    {
        throw std::invalid_argument("Invalid providers");
    }

    m_ETWMonitorThread = CreateThread(
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)&EtwMonitor::StartEtwMonitorStatic,
        this,
        0,
        nullptr
    );

    if (m_ETWMonitorThread == NULL)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateThread");
    }
}

EtwMonitor::~EtwMonitor()
{
    ULONG status;

    const std::wstring mySessionName = g_sessionName;
    PEVENT_TRACE_PROPERTIES petp = (PEVENT_TRACE_PROPERTIES)&this->m_vecStopTracePropsBuffer[0];

    status = ::ControlTraceW(m_startTraceHandle, mySessionName.c_str(), petp, EVENT_TRACE_CONTROL_STOP);
    if (status != ERROR_SUCCESS)
    {
        switch (status)
        {
        case ERROR_INVALID_PARAMETER:
            logWriter.TraceWarning(
                Utility::FormatString(L"Invalid TraceHandle or InstanceName is Null or both. Error: %lu", status).c_str()
            );
            break;
        case ERROR_ACCESS_DENIED:
            logWriter.TraceWarning(
                Utility::FormatString(L"Only users running with elevated administrative privileges can control event tracing sessions. Error: %lu", status).c_str()
            );
            break;
        case ERROR_WMI_INSTANCE_NOT_FOUND:
            logWriter.TraceWarning(
                Utility::FormatString(L"The given session is not running. Error: %lu", status).c_str()
            );
            break;
        case ERROR_ACTIVE_CONNECTIONS:
            logWriter.TraceWarning(
                Utility::FormatString(L"The session is already in the process of stopping. Error: %lu", status).c_str()
            );
            break;
        default:
            logWriter.TraceWarning(
                Utility::FormatString(L"Another issue might be preventing the stop of the event tracing session. Error: %lu", status).c_str()
            );
            break;
        }
    }
    

    CloseTrace(m_startTraceHandle);

    //
    // Wait for watch thread to exit.
    //
    this->m_stopFlag = true;

    DWORD waitResult = WaitForSingleObject(m_ETWMonitorThread, ETW_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS);

    if (waitResult != WAIT_OBJECT_0)
    {
        HRESULT hr = (waitResult == WAIT_FAILED) ? HRESULT_FROM_WIN32(GetLastError())
            : HRESULT_FROM_WIN32(waitResult);

        //
        // This object is being destroyed, so kill the thread to avoid an
        // access to the invalid object.
        //
        TerminateThread(m_ETWMonitorThread, 0);
    }

    if (m_ETWMonitorThread != NULL)
    {
        CloseHandle(m_ETWMonitorThread);
    }
}

///
/// Filter only the valid providers, that are the ones with a specified GUID, or have
/// a specified provider name AND this is is found in the system, using TdhEnumerateProviders
///
/// \param Providers        The ETWProvider specified by the user in the configuration.
/// \param ValidProviders   The providers that met the requirements listed above.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::FilterValidProviders(
    _In_ const std::vector<ETWProvider>& Providers,
    _Out_ std::vector<ETWProvider>& ValidProviders
    )
{
    std::map<wstring, ETWProvider> providersWithoutGuid;
    DWORD status = ERROR_SUCCESS;
    PROVIDER_ENUMERATION_INFO* penum = NULL;    // Buffer that contains provider information
    DWORD bufferSize = 0;                       // Size of the penum buffer

    for (auto provider : Providers)
    {
        if (provider.ProviderGuidStr.empty())
        {
            if (!provider.ProviderName.empty())
            {
                wstring providerName(provider.ProviderName);
                transform(
                    providerName.begin(), providerName.end(),
                    providerName.begin(),
                    towlower);
                providersWithoutGuid[providerName] = (provider);
            }
        }
        else
        {
            ValidProviders.push_back(provider);
        }
    }

    //
    // Return now if there aren't providers that need to obtain theirs GUID.
    //
    if (providersWithoutGuid.empty())
    {
        return status;
    }

    //
    // Retrieve the required buffer size.
    //
    status = TdhEnumerateProviders(penum, &bufferSize);

    //
    // Allocate the required buffer and call TdhEnumerateProviders. The list of
    // providers can change between the time you retrieved the required buffer
    // size and the time you enumerated the providers, so call TdhEnumerateProviders
    // in a loop until the function does not return ERROR_INSUFFICIENT_BUFFER.
    //
    while (ERROR_INSUFFICIENT_BUFFER == status)
    {
        PROVIDER_ENUMERATION_INFO* ptemp = (PROVIDER_ENUMERATION_INFO*)realloc(penum, bufferSize);
        if (NULL == ptemp)
        {
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to allocate memory to enumerate ETW providers. Size=%lu.",
                    bufferSize
                ).c_str()
            );
            status = ERROR_OUTOFMEMORY;
            break;
        }

        penum = ptemp;
        ptemp = NULL;

        status = TdhEnumerateProviders(penum, &bufferSize);
    }

    if (ERROR_SUCCESS != status)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to enumerate providers. Error: %lu.", status).c_str()
        );
    }
    else
    {
        //
        // Loop through the list of providers and print the provider's name, GUID,
        // and the source of the information (MOF class or instrumentation manifest).
        //
        for (DWORD i = 0; i < penum->NumberOfProviders; i++)
        {
            wstring providerName((LPWSTR)((PBYTE)(penum) + penum->TraceProviderInfoArray[i].ProviderNameOffset));
            transform(
                providerName.begin(), providerName.end(),
                providerName.begin(),
                towlower);

            auto providerPair = providersWithoutGuid.find(providerName);
            if (providerPair != providersWithoutGuid.end())
            {
                LPWSTR pStringGuid = NULL;
                HRESULT hr = StringFromCLSID(penum->TraceProviderInfoArray[i].ProviderGuid, &pStringGuid);

                if (FAILED(hr))
                {
                    continue;
                }

                providerPair->second.ProviderGuid = penum->TraceProviderInfoArray[i].ProviderGuid;
                providerPair->second.ProviderGuidStr = std::wstring(pStringGuid);


                CoTaskMemFree(pStringGuid);
            }
        }

        //
        // Add the providers that were possible to find its GUID
        //
        for (auto providerPair : providersWithoutGuid)
        {
            if (!providerPair.second.ProviderGuidStr.empty())
            {
                ValidProviders.push_back(providerPair.second);
            }
        }
    }

    if (penum)
    {
        free(penum);
        penum = NULL;
    }

    return status;
}


///
/// Entry for the spawned ETW monitor thread.
///
/// \param Context Callback context to the ETW monitor thread.
///                It's ETWMonitor object that started this thread.
///
/// \return Status of ETW monitoring operation.
///
DWORD
EtwMonitor::StartEtwMonitorStatic(
    _In_ LPVOID Context
    )
{
    auto pThis = reinterpret_cast<EtwMonitor*>(Context);
    try
    {
        DWORD status = pThis->StartEtwMonitor();
        if (status != ERROR_SUCCESS)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to start ETW monitor. Error: %lu", status).c_str()
            );
        }
        return status;
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start ETW monitor. %S", ex.what()).c_str()
        );
        return E_FAIL;
    }
    catch (...)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start ETW monitor").c_str()
        );
        return E_FAIL;
    }
}

///
/// Callback function to process an event.
///
/// \param EventRecord Pointer to EVENT_RECORD structure
///
void WINAPI EtwMonitor::OnEventRecordTramp(
    _In_ PEVENT_RECORD EventRecord
    )
{
    EtwMonitor* etwMon = static_cast<EtwMonitor*>(EventRecord->UserContext);
    try
    {
        if (etwMon != NULL)
        {
            DWORD status = etwMon->OnRecordEvent(EventRecord);
            if (status != ERROR_SUCCESS)
            {
                logWriter.TraceError(
                    Utility::FormatString(L"Failed to record ETW event. Error: %lu", status).c_str()
                );
            }
        }
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to record ETW event. %S", ex.what()).c_str()
        );
    }
    catch (...)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to record ETW event.").c_str()
        );
    }
}

///
/// Function automatically called by ProcessTrace when an ETW event arrives. If
/// it returns false, the ProcessTrace will stop receiving events.
///
BOOL WINAPI
EtwMonitor::StaticBufferEventCallback(
    _In_ PEVENT_TRACE_LOGFILE Buffer
    )
{
    try
    {
        EtwMonitor* etwMon = static_cast<EtwMonitor*>(Buffer->Context);
        return etwMon->BufferEventCallback(Buffer);
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to process ETW event callback. %S", ex.what()).c_str()
        );
        return FALSE;
    }
    catch (...)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to process ETW event callback. Unknown error occurred").c_str()
        );
        return FALSE;
    }
}

///
/// Returns false if the m_stopFlag is set true by the destructor,
/// causing ProcessTrace to stop.
///
BOOL WINAPI
EtwMonitor::BufferEventCallback(
    _In_ PEVENT_TRACE_LOGFILE Buffer
    )
{
    if (this->m_stopFlag)
    {
        return FALSE;
    }

    return TRUE;
}

///
/// Entry for the spawned ETW monitor thread. It starts a session and blocks
/// the current thread when ProcessTrace is called.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::StartEtwMonitor()
{
    DWORD status = ERROR_SUCCESS;
    EVT_HANDLE hSubscription = NULL;
    const DWORD eventsCount = 2;

    //
    // Setup the trace
    //
    status = StartTraceSession(m_startTraceHandle);
    if (status != ERROR_SUCCESS)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start ETW trace session. Error: %lu", status).c_str()
        );
        if (m_startTraceHandle != NULL)
        {
            CloseTrace(this->m_startTraceHandle);
        }

        return status;
    }

    EVENT_TRACE_LOGFILE trace;
    ZeroMemory(&trace, sizeof(EVENT_TRACE_LOGFILE));
    trace.LoggerName = (LPWSTR)g_sessionName.c_str();
    trace.LogFileName = (LPWSTR)NULL;
    trace.Context = this; // Wrap the current object, to call it from the callbacks.
    trace.EventRecordCallback = (PEVENT_RECORD_CALLBACK)(OnEventRecordTramp);
    trace.BufferCallback = (PEVENT_TRACE_BUFFER_CALLBACK)(StaticBufferEventCallback);
    trace.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;

    this->m_startTraceHandle = OpenTrace(&trace);
    if (INVALID_PROCESSTRACE_HANDLE == this->m_startTraceHandle)
    {
        status = GetLastError();

        logWriter.TraceError(
            Utility::FormatString(L"Failed to open ETW trace session. Error: %lu", status).c_str()
        );

        if (m_startTraceHandle != NULL)
        {
            CloseTrace(this->m_startTraceHandle);
        }

        return status;
    }

    //
    // This function blocks the thread
    //
    status = ProcessTrace(&m_startTraceHandle, 1, 0, 0);

    if (status != ERROR_SUCCESS && status != ERROR_CANCELLED)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to process ETW traces. Error: %lu", status).c_str()
        );
        CloseTrace(m_startTraceHandle);
    }

    return status;
}

///
/// Starts a session and enable the providers, specified by the user in the
/// configuration
///
/// \param TraceSessionHandle       The handle of the session started in this method.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::StartTraceSession(
    _Out_ TRACEHANDLE& TraceSessionHandle
    )
{
    const std::wstring mySessionName = g_sessionName;

    //
    // Start trace properties
    //
    this->m_vecEventTracePropsBuffer.resize(
        sizeof(EVENT_TRACE_PROPERTIES) + (mySessionName.length() + 1) * sizeof(mySessionName[0]));

    PEVENT_TRACE_PROPERTIES petp = (PEVENT_TRACE_PROPERTIES) &this->m_vecEventTracePropsBuffer[0];
    petp->Wnode.BufferSize = (ULONG)this->m_vecEventTracePropsBuffer.size();

    petp->Wnode.ClientContext = 1;  //use QPC for timestamp resolution
    petp->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    petp->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    petp->FlushTimer = 1;
    petp->LogFileNameOffset = 0;
    petp->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    //
    // Stop trace properties. This is needed because StopTrace returns performance
    // counters on it.
    //
    this->m_vecStopTracePropsBuffer.resize(
        sizeof(EVENT_TRACE_PROPERTIES) + 1024 * sizeof(WCHAR) + 1024 * sizeof(WCHAR));

    PEVENT_TRACE_PROPERTIES petpStop = (PEVENT_TRACE_PROPERTIES)& this->m_vecStopTracePropsBuffer[0];
    petpStop->Wnode.BufferSize = (ULONG)this->m_vecStopTracePropsBuffer.size();

    petpStop->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    petpStop->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + 1024 * sizeof(WCHAR);

    ULONG status = ::StartTrace(&TraceSessionHandle, mySessionName.c_str(), petp);
    if (ERROR_ALREADY_EXISTS == status)
    {
        //
        // If the session is active, is necessary to restart it, because the
        // configuration could change, and different providers could be listened.
        //
        status = ::StopTrace(TraceSessionHandle, mySessionName.c_str(), petpStop);
        if (status != ERROR_SUCCESS)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to stop ETW trace. Error: %lu", status).c_str()
            );
            return status;
        }

        status = ::StartTrace(&TraceSessionHandle, mySessionName.c_str(), petp);
    }

    if (status != ERROR_SUCCESS)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start ETW trace. Error: %lu", status).c_str()
        );
        TraceSessionHandle = 0L;
        return status;
    }
    else
    {
        //
        // Iterate through the providers to enable them
        //
        for (auto provider : m_providersConfig)
        {
            status = EnableTraceEx2(
                TraceSessionHandle,
                &provider.ProviderGuid,
                EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                provider.Level,
                provider.Keywords,
                0,
                0,
                NULL
            );

            if (status != ERROR_SUCCESS)
            {
                LPWSTR pwsProviderId = NULL;
                HRESULT hr = StringFromCLSID(provider.ProviderGuid, &pwsProviderId);

                if (FAILED(hr))
                {
                    logWriter.TraceError(
                        Utility::FormatString(L"Failed to convert GUID to string. Error: 0x%x", hr).c_str()
                    );
                }
                else
                {
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Failed to enable ETW trace session. Error: %lu, Provider GUID: %s",
                            status, pwsProviderId).c_str()
                    );

                    CoTaskMemFree(pwsProviderId);
                    pwsProviderId = NULL;
                }

                switch (status)
                {
                case ERROR_INVALID_PARAMETER:
                    logWriter.TraceError(L"The ProviderId id NULL or the TraceHandle is 0.");
                    break;
                case ERROR_TIMEOUT:
                    logWriter.TraceError(L"The timeout value expired before the enable callback completed.");
                    break;
                case ERROR_INVALID_FUNCTION:
                    logWriter.TraceError(L"You cannot update the level when the provider is not registered.");
                    break;
                case ERROR_NO_SYSTEM_RESOURCES:
                    logWriter.TraceError(L"Exceeded the number of ETW trace sessions that the provider can enable.");
                    break;
                case ERROR_ACCESS_DENIED:
                    logWriter.TraceError(L"Only users with administrative privileges can enable event providers to a cross-process session.");
                    break;
                default:
                    logWriter.TraceError(
                        Utility::FormatString(L"An unknown error occurred: %lu", status).c_str()
                    );
                    break;
                }

                return status;
            }
        }
    }

    return status;
}

///
/// Receives the event and print it if the GUID matches with the ones
/// specified in the configuration.
///
/// \param EventRecord      The event record received by EventRecordCallback
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::OnRecordEvent(
    _In_ const PEVENT_RECORD EventRecord
    )
{
    DWORD status = ERROR_SUCCESS;
    PTRACE_EVENT_INFO pInfo = NULL;
    bool skipEvent = true;
    std::vector<BYTE> eventBuffer;

    for (auto provider : m_providersConfig)
    {
        if (IsEqualGUID(EventRecord->EventHeader.ProviderId, provider.ProviderGuid))
        {
            skipEvent = false;
            break;
        }
    }

    if (!skipEvent)
    {
        DWORD bufferSize = 0;

        //
        // Retrieve the required buffer size for the event metadata.
        //
        status = TdhGetEventInformation(EventRecord, 0, NULL, pInfo, &bufferSize);

        if (ERROR_INSUFFICIENT_BUFFER == status)
        {
            try
            {
                eventBuffer.resize(bufferSize);
                pInfo = (TRACE_EVENT_INFO*)&eventBuffer.at(0);
                status = ERROR_SUCCESS;
            }
            catch (std::bad_alloc)
            {
                logWriter.TraceError(
                    Utility::FormatString(L"Failed to allocate memory for event info (size=%lu).", bufferSize).c_str()
                );
                status = ERROR_OUTOFMEMORY;
            }

            if (ERROR_SUCCESS == status)
            {
                //
                // Retrieve the event metadata.
                //
                status = TdhGetEventInformation(EventRecord, 0, NULL, pInfo, &bufferSize);
            }
        }

        if (ERROR_SUCCESS != status)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to query ETW event information. Error: %lu", status).c_str()
            );
        }


        //
        // Process all the event types, but WPP kind.
        //
        if (status == ERROR_SUCCESS &&
               (pInfo->DecodingSource == DecodingSourceXMLFile ||
                pInfo->DecodingSource == DecodingSourceWbem ||
                pInfo->DecodingSource == DecodingSourceTlg))
        {
            status = PrintEvent(EventRecord, pInfo);
            if (status != ERROR_SUCCESS)
            {
                logWriter.TraceError(
                    Utility::FormatString(L"Failed to print event. Error: %lu", status).c_str()
                );
            }
        }
    }

    return status;
}


///
/// Prints the data and metadata of the event.
///
/// \param EventRecord  The event record received by EventRecordCallback
/// \param EventInfo    A struct with event metadata.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::PrintEvent(
    _In_ const PEVENT_RECORD EventRecord,
    _In_ const PTRACE_EVENT_INFO EventInfo
    )
{
    DWORD status = ERROR_SUCCESS;

    try
    {
        std::wstring metadataStr;
        status = FormatMetadata(EventRecord, EventInfo, metadataStr);

        if (status != ERROR_SUCCESS)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to format ETW event metadata. Error: %lu", status).c_str()
            );
            return status;
        }

        std::wstring dataStr;
        status = FormatData(EventRecord, EventInfo, dataStr);

        if (status != ERROR_SUCCESS)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to format ETW event data. Error: %lu", status).c_str()
            );
            return status;
        }

        std::wstring formattedEvent = Utility::FormatString(
            L"<Source>EtwEvent</Source>%ls%ls",
            metadataStr.c_str(),
            dataStr.c_str());

        //
        // If the multi-line option is disabled, remove all new lines from the output.
        //
        if (!this->m_eventFormatMultiLine)
        {
            std::transform(formattedEvent.begin(), formattedEvent.end(), formattedEvent.begin(),
                [](WCHAR ch) {
                    switch (ch) {
                    case L'\r':
                    case L'\n':
                        return L' ';
                    }
                    return ch;
                });
        }

        logWriter.WriteConsoleLog(formattedEvent);
    }
    catch(std::bad_alloc&)
    {
        status = ERROR_NOT_ENOUGH_MEMORY;
    }
    catch(...)
    {
        status = ERROR_BAD_FORMAT;
    }

    return status;
}

///
/// Returns the properties(metadata) of the event in a wstring
///
DWORD
EtwMonitor::FormatMetadata(
    _In_ const PEVENT_RECORD EventRecord,
    _In_ const PTRACE_EVENT_INFO EventInfo,
    _Inout_ std::wstring& Result
    )
{
    std::wostringstream oss;
    FILETIME fileTime;
    LPWSTR pName = NULL;

    //
    // Format the time of the event
    //
    fileTime.dwHighDateTime = EventRecord->EventHeader.TimeStamp.HighPart;
    fileTime.dwLowDateTime = EventRecord->EventHeader.TimeStamp.LowPart;

    oss << L"<Time>" << Utility::FileTimeToString(fileTime).c_str() << L"</Time>";

    //
    // Format provider Name
    //
    if (EventInfo->ProviderNameOffset > 0) {
        pName = (LPWSTR)((PBYTE)(EventInfo)+EventInfo->ProviderNameOffset);
    }
    oss << L"<Provider Name=\"" << pName << "\"/>";

    //
    // Format provider Id
    //
    LPWSTR pwsProviderId = NULL;
    HRESULT hr = StringFromCLSID(EventRecord->EventHeader.ProviderId, &pwsProviderId);

    if (FAILED(hr))
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to convert ETW provider GUID to string. Error: 0x%x\n", hr).c_str()
        );
        return hr;
    }

    oss << L"<Provider idGuid=\"" << pwsProviderId << "\"/>";
    CoTaskMemFree(pwsProviderId);
    pwsProviderId = NULL;

    //
    // Names of the DecodingSource enum values
    //
    static const std::vector<std::wstring> c_DecodingSourceToString =
    {
        L"DecodingSourceXMLFile",
        L"DecodingSourceWbem",
        L"DecodingSourceWPP",
        L"DecodingSourceTlg",
        L"DecodingSourceMax",
    };

    oss << L"<DecodingSource>"
        << c_DecodingSourceToString[static_cast<UINT8>(EventInfo->DecodingSource)].c_str()
        << L"</DecodingSource>";

    oss << L"<Execution ProcessID=\""
        << EventRecord->EventHeader.ProcessId << "\" ThreadID=\""
        << EventRecord->EventHeader.ThreadId << "\" />";

    //
    // Print Level and Keyword
    //
    static const std::vector<std::wstring> c_LevelToString =
    {
        L"None",
        L"Critical",
        L"Error",
        L"Warning",
        L"Information",
        L"Verbose",
    };

    oss << L"<Level>"
        << c_LevelToString[EventRecord->EventHeader.EventDescriptor.Level]
        << L"</Level>";

    oss << L"<Keyword>"
        << Utility::FormatString(L"0x%llx", EventRecord->EventHeader.EventDescriptor.Keyword)
        << L"</Keyword>";


    //
    // Format specific metadata by type
    //
    if (DecodingSourceWbem == EventInfo->DecodingSource)  // MOF class
    {
        oss << L"<Provider Name=\"" << pName << "\"/>";

        LPWSTR pwsEventGuid = NULL;
        hr = StringFromCLSID(EventInfo->EventGuid, &pwsEventGuid);

        if (FAILED(hr))
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to convert GUID to string. Error: 0x%x\n", hr).c_str()
            );
            return hr;
        }

        oss << L"<EventID idGuid=\"" << pwsEventGuid << "\" />";;
        CoTaskMemFree(pwsEventGuid);
        pwsEventGuid = NULL;

        oss << L"<Version>" << EventRecord->EventHeader.EventDescriptor.Version << L"</Version>";
        oss << L"<Opcode>" << EventRecord->EventHeader.EventDescriptor.Opcode << L"</Opcode>";
    }
    else if (DecodingSourceXMLFile == EventInfo->DecodingSource) // Instrumentation manifest
    {
        oss << L"<EventID Qualifiers=\"" << (int)EventInfo->EventDescriptor.Id << "\">"
            << (int)EventInfo->EventDescriptor.Id << "</EventID>";
    }

    //
    // Convert the stream to a wstring
    //
    Result = oss.str();

    return ERROR_SUCCESS;
}

///
/// Formats the data of an event as a wstring with XML format.
///
/// \param EventRecord  The event record received by EventRecordCallback
/// \param EventInfo    A struct with event metadata.
/// \param Result       A string with the formatted data.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::FormatData(
    _In_ const PEVENT_RECORD EventRecord,
    _In_ const PTRACE_EVENT_INFO EventInfo,
    _Inout_ std::wstring& Result
    )
{
    DWORD status = ERROR_SUCCESS;
    std::wostringstream oss;

    if (EVENT_HEADER_FLAG_32_BIT_HEADER == (EventRecord->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER))
    {
        this->PointerSize = 4;
    }
    else
    {
        this->PointerSize = 8;
    }

    //
    // Print the event data for all the top-level properties. Metadata for all the
    // top-level properties come before structure member properties in the
    // property information array. If the EVENT_HEADER_FLAG_STRING_ONLY flag is set,
    // the event data is a null-terminated string, so just print it.
    //
    oss << L"<EventData>";
    if (EVENT_HEADER_FLAG_STRING_ONLY == (EventRecord->EventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY))
    {
        oss << (LPWSTR)EventRecord->UserData;
    }
    else
    {
        PBYTE pUserData = (PBYTE)EventRecord->UserData;
        PBYTE pEndOfUserData = (PBYTE)EventRecord->UserData + EventRecord->UserDataLength;

        for (USHORT i = 0; i < EventInfo->TopLevelPropertyCount; i++)
        {
            status = _FormatData(EventRecord, EventInfo, i, pUserData, pEndOfUserData, oss);
            if (ERROR_SUCCESS != status)
            {
                logWriter.TraceError(L"Failed to format ETW event user data..");

                return status;
            }
        }
    }
    oss << L"</EventData>";

    Result = oss.str();

    return ERROR_SUCCESS;
}

///
/// Recursive function that reads the properties of the event's data
/// an add them to the stream Result.
///
/// \param EventRecord      The event record received by EventRecordCallback
/// \param EventInfo        A struct with event metadata.
/// \param Index            The index of the property to read.
/// \param pStructureName   Used to retrieve the property if it is inside a struct.
/// \param StructIndex      Used to retrieve the property if it is inside a struct.
/// \param Result           A wide string stream, where the formatted values are appended.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::_FormatData(
    _In_ const PEVENT_RECORD EventRecord,
    _In_ const PTRACE_EVENT_INFO EventInfo,
    _In_ USHORT Index,
    _Inout_ PBYTE& UserData,
    _In_ PBYTE EndOfUserData,
    _Inout_ std::wostringstream& Result
    )
{
    DWORD status = ERROR_SUCCESS;
    DWORD lastMember = 0;  // Last member of a structure
    USHORT propertyLength = 0;
    USHORT arraySize = 0;
    DWORD formattedDataSize = 0;
    std::vector<BYTE> formattedData;
    USHORT userDataConsumed = 0;

    status = GetPropertyLength(EventRecord, EventInfo, Index, propertyLength);
    if (ERROR_SUCCESS != status)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to query ETW event property length. Error: %ul", status).c_str()
        );
        UserData = NULL;

        return status;
    }

    //
    // Get the size of the array if the property is an array.
    //
    status = GetArraySize(EventRecord, EventInfo, Index, arraySize);

    for (USHORT k = 0; k < arraySize; k++)
    {
        Result << "<" << (LPWSTR)((PBYTE)(EventInfo) + EventInfo->EventPropertyInfoArray[Index].NameOffset) << ">";

        //
        // If the property is a structure, print the members of the structure.
        //
        if ((EventInfo->EventPropertyInfoArray[Index].Flags & PropertyStruct) == PropertyStruct)
        {
            lastMember = EventInfo->EventPropertyInfoArray[Index].structType.StructStartIndex +
                EventInfo->EventPropertyInfoArray[Index].structType.NumOfStructMembers;

            for (USHORT j = EventInfo->EventPropertyInfoArray[Index].structType.StructStartIndex; j < lastMember; j++)
            {
                status = _FormatData(EventRecord, EventInfo, j, UserData, EndOfUserData, Result);
                if (ERROR_SUCCESS != status || UserData == NULL)
                {
                    logWriter.TraceError(L"Failed to format ETW event user data.");
                    break;
                }
            }
        }
        else if(propertyLength > 0 || (EndOfUserData - UserData) > 0)
        {
            PEVENT_MAP_INFO pMapInfo = NULL;

            //
            // If the property could be a map, try to get its info.
            //
            if (TDH_INTYPE_UINT32 == EventInfo->EventPropertyInfoArray[Index].nonStructType.InType &&
                EventInfo->EventPropertyInfoArray[Index].nonStructType.MapNameOffset != 0)
            {
                status = GetMapInfo(EventRecord,
                    (PWCHAR)((PBYTE)(EventInfo) + EventInfo->EventPropertyInfoArray[Index].nonStructType.MapNameOffset),
                    EventInfo->DecodingSource,
                    pMapInfo);

                if (ERROR_SUCCESS != status)
                {
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Failed to query ETW event property of type map. Error: %lu",
                            status
                        ).c_str()
                    );

                    if (pMapInfo)
                    {
                        free(pMapInfo);
                        pMapInfo = NULL;
                    }

                    break;
                }
            }

            //
            // Get the size of the buffer required for the formatted data.
            //
            status = TdhFormatProperty(
                EventInfo,
                pMapInfo,
                PointerSize,
                EventInfo->EventPropertyInfoArray[Index].nonStructType.InType,
                EventInfo->EventPropertyInfoArray[Index].nonStructType.OutType,
                propertyLength,
                (USHORT)(EndOfUserData - UserData),
                UserData,
                &formattedDataSize,
                (PWCHAR)formattedData.data(),
                &userDataConsumed);

            if (ERROR_INSUFFICIENT_BUFFER == status)
            {
                formattedData.resize(formattedDataSize);

                //
                // Retrieve the formatted data.
                //
                status = TdhFormatProperty(
                    EventInfo,
                    pMapInfo,
                    PointerSize,
                    EventInfo->EventPropertyInfoArray[Index].nonStructType.InType,
                    EventInfo->EventPropertyInfoArray[Index].nonStructType.OutType,
                    propertyLength,
                    (USHORT)(EndOfUserData - UserData),
                    UserData,
                    &formattedDataSize,
                    (PWCHAR)formattedData.data(),
                    &userDataConsumed);
            }

            if (pMapInfo)
            {
                free(pMapInfo);
                pMapInfo = NULL;
            }

            if (ERROR_SUCCESS == status)
            {
                Result << (PWCHAR)formattedData.data();

                UserData += userDataConsumed;
            }
            else
            {
                logWriter.TraceError(
                    Utility::FormatString(
                        L"Failed to format ETW event property value. Error: %lu",
                        status
                    ).c_str()
                );
                UserData = NULL;
                break;
            }
        }

        Result << "</" << (LPWSTR)((PBYTE)(EventInfo) + EventInfo->EventPropertyInfoArray[Index].NameOffset) << ">";
    }

    return status;
}

///
/// Get the length of the property data. For MOF-based events, the size is inferred from the data type
/// of the property. For manifest-based events, the property can specify the size of the property value
/// using the length attribute. The length attribute can specify the size directly or specify the name
/// of another property in the event data that contains the size. If the property does not include the
/// length attribute, the size is inferred from the data type. The length will be zero for variable
/// length, null-terminated strings and structures.
///
/// \param EventRecord  The event record received by EventRecordCallback
/// \param EventInfo    A struct with event metadata.
/// \param Index        Index of the property to request.
/// \param arraySize    Size of the property, obtained in this function.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::GetPropertyLength(
    _In_ const PEVENT_RECORD EventRecord,
    _In_ const PTRACE_EVENT_INFO EventInfo,
    _In_ const USHORT Index,
    _Out_ USHORT& PropertyLength
    )
{
    DWORD status = ERROR_SUCCESS;
    PROPERTY_DATA_DESCRIPTOR dataDescriptor;
    DWORD propertySize = 0;

    //
    // Initialize out parameter
    //
    PropertyLength = 0;

    //
    // If the property is a binary blob and is defined in a manifest, the property can
    // specify the blob's size or it can point to another property that defines the
    // blob's size. The PropertyParamLength flag tells you where the blob's size is defined.
    //
    if ((EventInfo->EventPropertyInfoArray[Index].Flags & PropertyParamLength) == PropertyParamLength)
    {
        DWORD length = 0;  // Expects the length to be defined by a UINT16 or UINT32
        DWORD j = EventInfo->EventPropertyInfoArray[Index].lengthPropertyIndex;
        ZeroMemory(&dataDescriptor, sizeof(PROPERTY_DATA_DESCRIPTOR));
        dataDescriptor.PropertyName = (ULONGLONG)((PBYTE)(EventInfo) + EventInfo->EventPropertyInfoArray[j].NameOffset);
        dataDescriptor.ArrayIndex = ULONG_MAX;
        status = TdhGetPropertySize(EventRecord, 0, NULL, 1, &dataDescriptor, &propertySize);
        status = TdhGetProperty(EventRecord, 0, NULL, 1, &dataDescriptor, propertySize, (PBYTE)&length);
        PropertyLength = (USHORT)length;
    }
    else
    {
        if (EventInfo->EventPropertyInfoArray[Index].length > 0)
        {
            PropertyLength = EventInfo->EventPropertyInfoArray[Index].length;
        }
        else
        {
            //
            // If the property is a binary blob and is defined in a MOF class, the extension
            // qualifier is used to determine the size of the blob. However, if the extension
            // is IPAddrV6, you must set the propertyLength variable yourself because the
            // EVENT_PROPERTY_INFO.length field will be zero.
            //
            if (TDH_INTYPE_BINARY == EventInfo->EventPropertyInfoArray[Index].nonStructType.InType &&
                TDH_OUTTYPE_IPV6 == EventInfo->EventPropertyInfoArray[Index].nonStructType.OutType)
            {
                PropertyLength = (USHORT)sizeof(IN6_ADDR);
            }
            else if (TDH_INTYPE_UNICODESTRING == EventInfo->EventPropertyInfoArray[Index].nonStructType.InType ||
                TDH_INTYPE_ANSISTRING == EventInfo->EventPropertyInfoArray[Index].nonStructType.InType ||
                (EventInfo->EventPropertyInfoArray[Index].Flags & PropertyStruct) == PropertyStruct)
            {
                PropertyLength = EventInfo->EventPropertyInfoArray[Index].length;
            }
            else
            {
                logWriter.TraceError(
                    Utility::FormatString(
                        L"Failed to format ETW event property. Unexpected length of 0 for intype %d and outtype %d",
                        EventInfo->EventPropertyInfoArray[Index].nonStructType.InType,
                        EventInfo->EventPropertyInfoArray[Index].nonStructType.OutType
                    ).c_str()
                );

                status = ERROR_EVT_INVALID_EVENT_DATA;
            }
        }
    }

    return status;
}

///
/// Gets the size of a TDH property
///
/// \param EventRecord  The event record received by EventRecordCallback
/// \param EventInfo    A struct with event metadata.
/// \param Index        Index of the array to request.
/// \param arraySize    Size of the array, obtained in this function.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::GetArraySize(
    _In_ const PEVENT_RECORD EventRecord,
    _In_ const PTRACE_EVENT_INFO EventInfo,
    _In_ const USHORT Index,
    _Out_ USHORT& ArraySize
    )
{
    DWORD status = ERROR_SUCCESS;
    PROPERTY_DATA_DESCRIPTOR dataDescriptor;
    DWORD propertySize = 0;

    if ((EventInfo->EventPropertyInfoArray[Index].Flags & PropertyParamCount) == PropertyParamCount)
    {
        DWORD count = 0;  // Expects the count to be defined by a UINT16 or UINT32
        DWORD j = EventInfo->EventPropertyInfoArray[Index].countPropertyIndex;
        ZeroMemory(&dataDescriptor, sizeof(PROPERTY_DATA_DESCRIPTOR));
        dataDescriptor.PropertyName = (ULONGLONG)((PBYTE)(EventInfo) + EventInfo->EventPropertyInfoArray[j].NameOffset);
        dataDescriptor.ArrayIndex = ULONG_MAX;
        status = TdhGetPropertySize(EventRecord, 0, NULL, 1, &dataDescriptor, &propertySize);
        if (status != ERROR_SUCCESS)
        {
            ArraySize = 0;
            return ERROR_SUCCESS;
        }
        status = TdhGetProperty(EventRecord, 0, NULL, 1, &dataDescriptor, propertySize, (PBYTE)& count);
        ArraySize = (USHORT)count;
    }
    else
    {
        ArraySize = EventInfo->EventPropertyInfoArray[Index].count;
    }

    return status;
}

///
/// Gets the values of the Map property
///
/// \param EventRecord      The event record received by EventRecordCallback
/// \param MapName          The index of the map to request.
/// \param DecodingSource   The decoding type of the current event.
/// \param MapInfo          The map's data obtained in this function.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EtwMonitor::GetMapInfo(
    _In_ const PEVENT_RECORD EventRecord,
    _In_ LPWSTR MapName,
    _In_ DWORD DecodingSource,
    _Out_ PEVENT_MAP_INFO& MapInfo
    )
{
    DWORD status = ERROR_SUCCESS;
    DWORD mapSize = 0;

    //
    // Retrieve the required buffer size for the map info.
    //
    status = TdhGetEventMapInformation(EventRecord, MapName, MapInfo, &mapSize);

    if (ERROR_INSUFFICIENT_BUFFER == status)
    {
        MapInfo = (PEVENT_MAP_INFO)malloc(mapSize);
        if (MapInfo == NULL)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to allocate memory for ETW event map info (size=%lu).", mapSize).c_str()
            );
            status = ERROR_OUTOFMEMORY;
            return status;
        }

        //
        // Retrieve the map info.
        //
        status = TdhGetEventMapInformation(EventRecord, MapName, MapInfo, &mapSize);
    }

    if (ERROR_SUCCESS == status)
    {
        if (DecodingSourceXMLFile == DecodingSource)
        {
            RemoveTrailingSpace(MapInfo);
        }
    }
    else
    {
        if (ERROR_NOT_FOUND == status)
        {
            status = ERROR_SUCCESS; // This case is okay.
        }
        else
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to query ETW event information. Error: %lu.", status).c_str()
            );
        }
    }

    return status;
}

///
/// In XML decoding, there are trailing spaces. This functions removes them.
///
inline
void
EtwMonitor::RemoveTrailingSpace(
    _In_ PEVENT_MAP_INFO MapName
    )
{
    size_t byteLength = 0;

    for (DWORD i = 0; i < MapName->EntryCount; i++)
    {
        byteLength = (wcslen((LPWSTR)((PBYTE)MapName + MapName->MapEntryArray[i].OutputOffset)) - 1) * 2;
        *((LPWSTR)((PBYTE)MapName + (MapName->MapEntryArray[i].OutputOffset + byteLength))) = L'\0';
    }
}
