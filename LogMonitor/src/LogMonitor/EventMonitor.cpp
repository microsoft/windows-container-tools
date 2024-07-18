//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

using namespace std;


///
/// EventMonitor.cpp
///
/// Monitors a event log for new events in specified channels.
///
/// EventMonitor starts a monitor thread that waits for events from EvtSubscribe
/// or for a stop event to be set. The same thread processes the changes and invokes the callback.
/// Note: The constructor blocks until the spawned thread starts listening for changes or dies.
///
/// The destructor signals the stop event and waits up to MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS for the monitoring
/// thread to exit. To prevent the thread from out-living EventMonitor, the destructor fails fast
/// if the wait fails or times out. This also ensures the callback is not being called and will not be
/// called once EventMonitor is destroyed.
///


EventMonitor::EventMonitor(
    _In_ const std::vector<EventLogChannel>& EventChannels,
    _In_ bool EventFormatMultiLine,
    _In_ bool StartAtOldestRecord,
    _In_ std::wstring LogFormat,
    _In_ std::wstring CustomLogFormat = L""
    ) :
    m_eventChannels(EventChannels),
    m_eventFormatMultiLine(EventFormatMultiLine),
    m_startAtOldestRecord(StartAtOldestRecord),
    m_logFormat(LogFormat),
    m_customLogFormat(CustomLogFormat)
{
    m_stopEvent = NULL;
    m_eventMonitorThread = NULL;

    m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    if (!m_stopEvent)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    m_eventMonitorThread = CreateThread(
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)&EventMonitor::StartEventMonitorStatic,
        this,
        0,
        nullptr
    );

    if (!m_eventMonitorThread)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateThread");
    }
}

EventMonitor::~EventMonitor()
{
    if (!SetEvent(m_stopEvent))
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to gracefully stop event log monitor %lu", GetLastError()).c_str()
        );
    }
    else
    {
        //
        // Wait for watch thread to exit.
        //
        DWORD waitResult = WaitForSingleObject(m_eventMonitorThread, EVENT_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS);

        if (waitResult != WAIT_OBJECT_0)
        {
            HRESULT hr = (waitResult == WAIT_FAILED) ? HRESULT_FROM_WIN32(GetLastError())
                                                           : HRESULT_FROM_WIN32(waitResult);
        }
    }

    if (!m_eventMonitorThread)
    {
        CloseHandle(m_eventMonitorThread);
    }

    if (!m_stopEvent)
    {
        CloseHandle(m_stopEvent);
    }
}

///
/// Entry for the spawned event monitor thread.
///
/// \param Context Callback context to the event monitor thread.
///                It's EventMonitor object that started this thread.
///
/// \return Status of event monitoring operation.
///
DWORD
EventMonitor::StartEventMonitorStatic(
    _In_ LPVOID Context
    )
{
    auto pThis = reinterpret_cast<EventMonitor*>(Context);
    try
    {
        DWORD status = pThis->StartEventMonitor();
        if (status != ERROR_SUCCESS)
        {
            logWriter.TraceError(
                Utility::FormatString(L"Failed to start event log monitor. Error: %lu", status).c_str()
            );
        }
        return status;
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start event log monitor. %S", ex.what()).c_str()
        );
        return E_FAIL;
    }
    catch (...)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to start event log monitor. Unknown error occurred.").c_str()
        );
        return E_FAIL;
    }
}


///
/// Entry for the spawned event monitor thread. Loops to wait for either the stop event in which case it exits
/// or for events to be arrived. When new events are arrived, it invokes the callback, resets,
/// and starts the wait again.
///
/// \return Status of event monitoring operation.
///
DWORD
EventMonitor::StartEventMonitor()
{
    DWORD status = ERROR_SUCCESS;
    EVT_HANDLE hSubscription = NULL;
    const LPWSTR pwsQuery = (LPWSTR)(L"*");
    const DWORD eventsCount = 2;

    EnableEventLogChannels();

    //
    // Order stop event first so that stop is prioritized if both events are already signalled (changes
    // are available but stop has been called).
    //
    HANDLE aWaitHandles[eventsCount];

    aWaitHandles[0] = m_stopEvent;

    //
    // Get a handle to a manual reset event object that the subscription will signal
    // when events become available that match your query criteria.
    //
    HANDLE subscEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if(!subscEvent)
    {
        return GetLastError();
    }
    aWaitHandles[1] = subscEvent;

    //
    // Subscribe to events.
    //
    DWORD evtSubscribeFlags = this->m_startAtOldestRecord ?
        EvtSubscribeStartAtOldestRecord : EvtSubscribeToFutureEvents;

    hSubscription = EvtSubscribe(
        NULL,
        aWaitHandles[1],
        NULL,
        ConstructWindowsEventQuery(m_eventChannels).c_str(),
        NULL,
        NULL,
        NULL,
        evtSubscribeFlags
    );

    if (NULL == hSubscription)
    {
        status = GetLastError();

        if (ERROR_EVT_CHANNEL_NOT_FOUND == status)
            logWriter.TraceError(L"Failed to subscribe to event log channel. The specified event channel was not found.");
        else if (ERROR_EVT_INVALID_QUERY == status)
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to subscribe to event log channel. Event query %s is not valid.",
                    pwsQuery).c_str()
            );
        else
            logWriter.TraceError(
                Utility::FormatString(L"Failed to subscribe to event log channel. Error: %lu.", status).c_str()
            );
    }

    if (status == ERROR_SUCCESS)
    {
        while (true)
        {
            DWORD wait = WaitForMultipleObjects(eventsCount, aWaitHandles, FALSE, INFINITE);

            if (0 == wait - WAIT_OBJECT_0)  // Console input
            {
                break;
            }
            else if (1 == wait - WAIT_OBJECT_0) // Query results
            {
                if (ERROR_NO_MORE_ITEMS != (status = EnumerateResults(hSubscription)))
                {
                    break;
                }

                status = ERROR_SUCCESS;
                ResetEvent(aWaitHandles[1]);
            }
            else
            {
                if (WAIT_FAILED == wait)
                {
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Failed to subscribe to event log channel. Wait operation on event handle failed. Error: %lu.",
                            GetLastError()).c_str()
                    );
                }
                break;
            }
        }
    }

    if (hSubscription)
    {
        EvtClose(hSubscription);
    }

    if(subscEvent)
    {
        CloseHandle(subscEvent);
    }

    return status;
}


///
/// Constructs and returns an XML Query for Windows Event collection using the supplied parameters.
///
/// \param EventChannels         Supplies the event channels to query.
///
/// \return XML Query for Windows Event collection.
///
std::wstring
EventMonitor::ConstructWindowsEventQuery(
    _In_ const std::vector<EventLogChannel>& EventChannels
    )
{
    //
    // Construct the query
    //
    std::wstring query = LR"(<QueryList>)";
    query += LR"(<Query Id="0" Path="System">)";

    for (const auto& eventChannel : EventChannels)
    {
        //
        // Construct the query portion for the log level
        //
        std::wstring logLevelQuery = L"";
        logLevelQuery = L"(";

        if (eventChannel.Level >= EventChannelLogLevel::Critical)
        {
            logLevelQuery += L"Level=1 or ";
        }
        if (eventChannel.Level >= EventChannelLogLevel::Error)
        {
            logLevelQuery += L"Level=2 or ";
        }
        if (eventChannel.Level >= EventChannelLogLevel::Warning)
        {
            logLevelQuery += L"Level=3 or ";
        }
        if (eventChannel.Level >= EventChannelLogLevel::Information)
        {
            logLevelQuery += L"Level=4 or ";
        }
        if (eventChannel.Level >= EventChannelLogLevel::Verbose)
        {
            logLevelQuery += L"Level=5 or ";
        }

        //
        // Remove last ' or '
        //
        logLevelQuery.erase(logLevelQuery.size() - 4);
        logLevelQuery += L")";

        query += Utility::FormatString(
            LR"(<Select Path="%s">*[System[%s)", eventChannel.Name.c_str(), logLevelQuery.c_str());

        query += LR"(]]</Select>)";
    }

    query += LR"(</Query>)";
    query += LR"(</QueryList>)";

    return query;
}


///
/// Enumerate the events in the result set.
///
/// \param EventChannels         The handle to the subscription that EvtSubscribe function returned.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EventMonitor::EnumerateResults(
    _In_ EVT_HANDLE hResults
    )
{
    DWORD status = ERROR_SUCCESS;
    EVT_HANDLE hEvents[EVENT_ARRAY_SIZE];
    DWORD dwReturned = 0;

    while (true)
    {
        //
        // Get a block of events from the result set.
        //
        if (!EvtNext(hResults, EVENT_ARRAY_SIZE, hEvents, INFINITE, 0, &dwReturned))
        {
            if (ERROR_NO_MORE_ITEMS != (status = GetLastError()))
            {
                logWriter.TraceError(
                    Utility::FormatString(L"Failed to query next event. Error: %lu.", status).c_str()
                );
            }

            goto cleanup;
        }

        //
        // For each event, call the PrintEvent function which renders the
        // event for display.
        //
        for (DWORD i = 0; i < dwReturned; i++)
        {
            status = PrintEvent(hEvents[i]);

            if (ERROR_SUCCESS != status)
            {
                logWriter.TraceWarning(
                    Utility::FormatString(
                        L"Failed to render event log event. The event will not be processed. Error: %lu.",
                        status
                    ).c_str()
                );
                status = ERROR_SUCCESS;
            }

            EvtClose(hEvents[i]);
            hEvents[i] = NULL;
        }
    }

cleanup:

    //
    // Closes any events in case an error occurred above.
    //
    for (DWORD i = 0; i < dwReturned; i++)
    {
        if (NULL != hEvents[i])
        {
            EvtClose(hEvents[i]);
        }
    }

    return status;
}

///
/// Constructs an EventLog object with the contents of an event's value paths.
///
/// \param EventHandle  Supplies a handle to an event, used to extract value paths.
/// \param AdditionalValuePaths  Supplies additional values to include in the returned EventLog object.
///
/// \return DWORD
///
DWORD
EventMonitor::PrintEvent(
    _In_ const HANDLE& EventHandle
    )
{
    DWORD status = ERROR_SUCCESS;
    DWORD bytesWritten = 0;
    EVT_HANDLE renderContext = NULL;
    EVT_HANDLE publisher = NULL;

    // struct to hold the Event log entry and later format print
    EventLogEntry logEntry;
    EventLogEntry* pLogEntry = &logEntry;

    static constexpr LPCWSTR defaultValuePaths[] = {
        L"Event/System/Provider/@Name",
        L"Event/System/Channel",
        L"Event/System/EventID",
        L"Event/System/Level",
        L"Event/System/TimeCreated/@SystemTime",
    };

    static const std::vector<std::wstring> c_LevelToString =
    {
        L"Unknown",
        L"Critical",
        L"Error",
        L"Warning",
        L"Information",
        L"Verbose",
    };

    static const DWORD defaultValuePathsCount = sizeof(defaultValuePaths) / sizeof(LPCWSTR);

    try
    {
        //
        // Construct the value paths that will be used to query the events
        //
        std::vector<LPCWSTR> valuePaths(defaultValuePaths, defaultValuePaths + defaultValuePathsCount);

        //
        // Collect event system properties
        //
        renderContext = EvtCreateRenderContext(
            static_cast<DWORD>(valuePaths.size()),
            &valuePaths[0],
            EvtRenderContextValues
        );

        if (!renderContext)
        {
            return GetLastError();
        }

        DWORD propertyCount = 0;
        DWORD bufferSize = 0;
        std::vector<EVT_VARIANT> variants;

        EvtRender(renderContext, EventHandle, EvtRenderEventValues, 0, nullptr, &bufferSize, &propertyCount);
        if (ERROR_INVALID_HANDLE == GetLastError())
        {
            status = ERROR_INVALID_HANDLE;
        }

        status = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == status || ERROR_EVT_UNRESOLVED_VALUE_INSERT == status)
        {
            status = ERROR_SUCCESS;
        }

        if (status == ERROR_SUCCESS)
        {
            //
            // Allocate more memory to accommodate modulus
            //
            variants.resize((bufferSize / sizeof(EVT_VARIANT)) + 1, EVT_VARIANT{});
            if(!EvtRender(
                renderContext,
                EventHandle,
                EvtRenderEventValues,
                bufferSize,
                &variants[0],
                &bufferSize,
                &propertyCount))
            {
                status = GetLastError();

                logWriter.TraceError(
                    Utility::FormatString(L"Failed to render event. Error: %lu", status).c_str()
                );
            }
        }

        if (status == ERROR_SUCCESS)
        {
            //
            // Extract the variant values for each queried property. If the variant failed to get a valid type
            // set a default value.
            //
            std::wstring providerName = (EvtVarTypeString != variants[0].Type) ? L"" : variants[0].StringVal;
            std::wstring channelName = (EvtVarTypeString != variants[1].Type) ? L"" : variants[1].StringVal;
            pLogEntry->eventId = (EvtVarTypeUInt16 != variants[2].Type) ? 0 : variants[2].UInt16Val;
            UINT8 level = (EvtVarTypeByte != variants[3].Type) ? 0 : variants[3].ByteVal;
            ULARGE_INTEGER fileTimeAsInt{};
            fileTimeAsInt.QuadPart = (EvtVarTypeFileTime != variants[4].Type) ? 0 : variants[4].FileTimeVal;
            FILETIME fileTimeCreated{
                fileTimeAsInt.LowPart,
                fileTimeAsInt.HighPart
            };

            //
            // Collect user message
            //
            publisher = EvtOpenPublisherMetadata(nullptr, providerName.c_str(), nullptr, 0, 0);

            if (publisher)
            {
                EvtFormatMessage(publisher, EventHandle, 0, 0, nullptr, EvtFormatMessageEvent, 0, nullptr, &bufferSize);
                status = GetLastError();

                if (status != ERROR_EVT_MESSAGE_NOT_FOUND)
                {
                    if (ERROR_INSUFFICIENT_BUFFER == status || ERROR_EVT_UNRESOLVED_VALUE_INSERT == status)
                    {
                        status = ERROR_SUCCESS;
                    }

                    if (m_eventMessageBuffer.capacity() < bufferSize)
                    {
                        m_eventMessageBuffer.resize(bufferSize);
                    }

                    if (!EvtFormatMessage(
                        publisher,
                        EventHandle,
                        0,
                        0,
                        nullptr,
                        EvtFormatMessageEvent,
                        bufferSize,
                        &m_eventMessageBuffer[0],
                        &bufferSize))
                    {
                        status = GetLastError();
                    }
                }
                else
                {
                    status = ERROR_SUCCESS;
                }
            }

            if (status == ERROR_SUCCESS)
            {
                pLogEntry->source = L"EventLog";
                pLogEntry->eventTime = Utility::FileTimeToString(fileTimeCreated);
                pLogEntry->eventChannel = channelName;
                pLogEntry->eventLevel = c_LevelToString[static_cast<UINT8>(level)];
                pLogEntry->eventMessage = (LPWSTR)(&m_eventMessageBuffer[0]);

                std::wstring formattedEvent;
                if (Utility::CompareWStrings(m_logFormat, L"Custom")) {
                    formattedEvent = Utility::FormatEventLineLog(m_customLogFormat, pLogEntry, pLogEntry->source);
                } else {
                    std::wstring logFmt;
                    if (Utility::CompareWStrings(m_logFormat, L"XML")) {
                        logFmt = L"<Log><Source>%s</Source><LogEntry><Time>%s</Time>"
                                 L"<Channel>%s</Channel><Level>%s</Level>"
                                 L"<EventId>%u</EventId><Message>%s</Message>"
                                 L"</LogEntry></Log>";
                    } else {
                        logFmt = L"{\"Source\": \"%s\","
                                 L"\"LogEntry\": {"
                                 L"\"Time\": \"%s\","
                                 L"\"Channel\": \"%s\","
                                 L"\"Level\": \"%s\","
                                 L"\"EventId\": %u,"
                                 L"\"Message\": \"%s\""
                                 L"}}";

                        // sanitize message
                        std::wstring msg(m_eventMessageBuffer.begin(), m_eventMessageBuffer.end());
                        Utility::SanitizeJson(msg);
                        pLogEntry->eventMessage = msg;
                    }

                    formattedEvent = Utility::FormatString(
                        logFmt.c_str(),
                        pLogEntry->source.c_str(),
                        pLogEntry->eventTime.c_str(),
                        pLogEntry->eventChannel.c_str(),
                        pLogEntry->eventLevel.c_str(),
                        pLogEntry->eventId,
                        pLogEntry->eventMessage.c_str()
                    );
                }

                logWriter.WriteConsoleLog(formattedEvent);
            }
        }
    }
    catch(...)
    {
        logWriter.TraceWarning(L"Failed to render event log event. The event will not be processed.");
    }

    if (publisher)
    {
        EvtClose(publisher);
    }

    if (renderContext)
    {
        EvtClose(renderContext);
    }

    return status;
}


/// Enables all monitored event log channels.
///
/// \return None
///
void
EventMonitor::EnableEventLogChannels()
{
    for (const auto& eventChannel : m_eventChannels) {
        DWORD status = EnableEventLogChannel(eventChannel.Name.c_str());

        if (status == RPC_S_SERVER_UNAVAILABLE) {
            HANDLE timerEvent = CreateWaitableTimer(NULL, FALSE, NULL);

            if (!timerEvent) {
            status = GetLastError();
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to create timer object.", status).c_str());
            break;
            }

            std::double_t waitInSeconds = 300;

            int elapsedTime = 0;

            const int eventsCount = 2;
            HANDLE channelEnableEvents[eventsCount] = {m_stopEvent, timerEvent};

            while (elapsedTime < waitInSeconds) {
                int waitInterval = Utility::GetWaitInterval(waitInSeconds, elapsedTime);
                LARGE_INTEGER timeToWait = Utility::ConvertWaitIntervalToLargeInt(waitInterval);
                BOOL waitableTimer = SetWaitableTimer(timerEvent, &timeToWait, 0, NULL, NULL, 0);
                if (!waitableTimer) {
                    status = GetLastError();
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Failed to set timer object to enable %s event channel. Error: %lu",
                            eventChannel.Name.c_str(),
                            status).c_str());
                    break;
                }

                DWORD wait = WaitForMultipleObjects(eventsCount, channelEnableEvents, FALSE, INFINITE);
                switch (wait) {
                    case WAIT_OBJECT_0:
                    {
                        //
                        // The process is exiting. Stop the timer and return.
                        //
                        CancelWaitableTimer(timerEvent);
                        CloseHandle(timerEvent);
                    }

                    case WAIT_OBJECT_0 + 1:
                    {
                        //
                        // Timer event. Retry enabling the failing channel.
                        //
                        break;
                    }

                    default:
                    {
                        //
                        // Wait failed, return the failure.
                        //
                        status = GetLastError();

                        logWriter.TraceError(
                            Utility::FormatString(
                                L"Failed to enable event channel. Channel: %ws Error: 0x%X",
                                eventChannel.Name.c_str(), status).c_str());

                        CancelWaitableTimer(timerEvent);
                        CloseHandle(timerEvent);
                    }
                }

                DWORD status = EnableEventLogChannel(eventChannel.Name.c_str());

                if (status == RPC_S_SERVER_UNAVAILABLE) {
                    elapsedTime += Utility::WAIT_INTERVAL;
                } else {
                    logWriter.TraceInfo(
                        Utility::FormatString(
                            L"Enabled %s event channel after %d seconds.",
                            eventChannel.Name.c_str(),
                            elapsedTime).c_str() );
                    status = ERROR_SUCCESS;
                    break;
                }
            }

            CancelWaitableTimer(timerEvent);
            CloseHandle(timerEvent);
        }
    }
}



/// Enables or disables an Event Log channel.
///
/// \param ChannelPath  Full path to the event log channel.
///
/// \return None
///
DWORD
EventMonitor::EnableEventLogChannel(
    _In_ LPCWSTR ChannelPath
    )
{
    DWORD       status = ERROR_SUCCESS;
    EVT_HANDLE  channelConfig = NULL;
    EVT_VARIANT propValue;
    DWORD dwPropValSize;

    //
    // Open the channel configuration.
    //
    channelConfig = EvtOpenChannelConfig(NULL, ChannelPath, 0);
    if (NULL == channelConfig) {
        status = GetLastError();
        goto Exit;
    }

    if (EvtGetChannelConfigProperty(channelConfig,
        EvtChannelConfigEnabled,
        0,
        sizeof(EVT_VARIANT),
        &propValue,
        &dwPropValSize)) {
        //
        // Return if event channel is already enabled.
        //
        if (propValue.BooleanVal) {
            goto Exit;
        }
    } else {
        status = GetLastError();
        logWriter.TraceError(
            Utility::FormatString(
                L"Failed to query event channel configuration. Channel: %ws Error: 0x%X",
                ChannelPath,
                status).c_str());
    }

    //
    // Set the Enabled property.
    //
    propValue.Type = EvtVarTypeBoolean;
    propValue.Count = 1;
    propValue.BooleanVal = true;

    if (!EvtSetChannelConfigProperty(
        channelConfig,
        EvtChannelConfigEnabled,
        0,
        &propValue)) {
        status = GetLastError();
        goto Exit;
    }

    //
    // Save changes.
    //
    if (!EvtSaveChannelConfig(channelConfig, 0)) {
        status = GetLastError();
        if (status == ERROR_EVT_INVALID_OPERATION_OVER_ENABLED_DIRECT_CHANNEL) {
            //
            // The channel is already enabled.
            //
            status = ERROR_SUCCESS;
        } else {
            goto Exit;
        }
    }

    status = ERROR_SUCCESS;

Exit:

    if (ERROR_SUCCESS != status) {
        logWriter.TraceInfo(
            Utility::FormatString(L"Waiting for %ws event channel to be enabled",
            ChannelPath).c_str());
    }

    if (channelConfig != NULL) {
        EvtClose(channelConfig);
    }

    return status;
}

std::wstring EventMonitor::EventFieldsMapping(_In_ std::wstring eventField, _In_ void* pLogEntryData)
{
    std::wostringstream oss;
    EventLogEntry* pLogEntry = (EventLogEntry*)pLogEntryData;

    if (Utility::CompareWStrings(eventField, L"TimeStamp")) oss << pLogEntry->eventTime;
    if (Utility::CompareWStrings(eventField, L"Severity")) oss << pLogEntry->eventLevel;
    if (Utility::CompareWStrings(eventField, L"Source")) oss << pLogEntry->source;
    if (Utility::CompareWStrings(eventField, L"EventID")) oss << pLogEntry->eventId;
    if (Utility::CompareWStrings(eventField, L"Message")) oss << pLogEntry->eventMessage;

    return oss.str();
}
