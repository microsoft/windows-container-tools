//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "Version.h"

using namespace std;

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "ws2_32.lib")  // For ntohs function
#pragma comment(lib, "shlwapi.lib")

#define ARGV_OPTION_CONFIG_FILE L"/Config"
#define ARGV_OPTION_HELP1 L"/?"
#define ARGV_OPTION_HELP2 L"--help"

LogWriter logWriter;

HANDLE g_hStopEvent = INVALID_HANDLE_VALUE;

std::unique_ptr<EventMonitor> g_eventMon(nullptr);
std::vector<std::shared_ptr<LogFileMonitor>> g_logfileMonitors;
std::unique_ptr<EtwMonitor> g_etwMon(nullptr);
std::wstring logFormat, processMonitorCustomFormat;

/// Handle signals.
///
/// \return None
///
void signalHandler(int signum) {
    logWriter.TraceError(
        Utility::FormatString(L"Catastrophic failure! Signal received: %d", signum).c_str()
    );

    // Terminate the program
    std::exit(signum);
}

/// Set up a signal handler
///
/// \return None
///
void setSignalHandler(int signum, void (*handler)(int)) {
    std::signal(signum, handler);
}

BOOL WINAPI ControlHandle(_In_ DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        {
            wprintf(L"\nCTRL signal received. The process will now terminate.\n");

            SetEvent(g_hStopEvent);
            g_hStopEvent = INVALID_HANDLE_VALUE;

            //
            // Propagate the CTRL signal
            //
            SetConsoleCtrlHandler(NULL, TRUE);
            GenerateConsoleCtrlEvent(dwCtrlType, 0);

            break;
        }

        default:
            return TRUE;
    }

    return TRUE;
}

void PrintUsage()
{
    wprintf(
        L"\n\tLogMonitor Tool Version %d.%d.%d \n\n",
        LM_MAJORNUMBER,
        LM_MINORNUMBER,
        LM_PATCHNUMBER
    );
    wprintf(L"\tUsage: LogMonitor.exe [/?] | [--help] | [[/CONFIG <PATH>][COMMAND [PARAMETERS]]] \n\n");
    wprintf(L"\t/?|--help   Shows help information\n");
    wprintf(L"\t<PATH>      Specifies the path of the Json configuration file. This is\n");
    wprintf(L"\t            an optional parameter. If not specified, then default Json\n");
    wprintf(L"\t            configuration file path %ws is used\n", DEFAULT_CONFIG_FILENAME);
    wprintf(L"\tCOMMAND     Specifies the name of the executable to be run \n");
    wprintf(L"\tPARAMETERS  Specifies the parameters to be passed to the COMMAND \n\n");
    wprintf(L"\tThis tool monitors Event log, ETW providers and log files and write the log entries\n");
    wprintf(L"\tto the console. The configuration of input log sources is specified in a Json file.\n");
    wprintf(L"\tfile.\n\n");
}

/// <summary>
/// Initialize the Event Log Monitor
/// </summary>
/// <param name="sourceEventLog">The EventLog source settings</param>
/// <param name="eventChannels">The vector to store EventLog channels</param>
void InitializeEventLogMonitor(
    std::shared_ptr<SourceEventLog> sourceEventLog,
    std::vector<EventLogChannel>& eventChannels,
    bool& eventMonMultiLine,
    bool& eventMonStartAtOldestRecord,
    std::wstring& eventCustomLogFormat)
{
    for (auto channel : sourceEventLog->Channels)
    {
        eventChannels.push_back(channel);
    }

    eventMonMultiLine = sourceEventLog->EventFormatMultiLine;
    eventMonStartAtOldestRecord = sourceEventLog->StartAtOldestRecord;
    eventCustomLogFormat = sourceEventLog->CustomLogFormat;
}

/// <summary>
/// Initialize the File Monitor
/// </summary>
/// <param name="sourceFile">The File source settings</param>
void InitializeFileMonitor(std::shared_ptr<SourceFile> sourceFile)
{
    try
    {
        std::shared_ptr<LogFileMonitor> logfileMon = make_shared<LogFileMonitor>(
            sourceFile->Directory,
            sourceFile->Filter,
            sourceFile->IncludeSubdirectories,
            sourceFile->WaitInSeconds,
            logFormat,
            sourceFile->CustomLogFormat
        );
        g_logfileMonitors.push_back(std::move(logfileMon));
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"Instantiation of a LogFileMonitor object failed for directory %ws. %S",
                sourceFile->Directory.c_str(),
                ex.what()
            ).c_str()
        );
    }
    catch (...)
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"Instantiation of a LogFileMonitor object failed for directory %ws. Unknown error occurred."
            ).c_str()
        );
    }
}

/// <summary>
/// Initialize the ETW Monitor
/// </summary>
/// <param name="sourceETW">The ETW source settings</param>
/// <param name="etwProviders">The vector to store ETW providers</param>
void InitializeEtwMonitor(
    std::shared_ptr<SourceETW> sourceETW,
    std::vector<ETWProvider>& etwProviders,
    bool& etwMonMultiLine,
    std::wstring& etwCustomLogFormat)
{
    for (auto provider : sourceETW->Providers)
    {
        etwProviders.push_back(provider);
    }

    etwMonMultiLine = sourceETW->EventFormatMultiLine;
    etwCustomLogFormat = sourceETW->CustomLogFormat;
}

/// <summary>
/// Initialize the Process Monitor
/// </summary>
/// <param name="sourceProcess">The Process source settings</param>
void InitializeProcessMonitor(std::shared_ptr<SourceProcess> sourceProcess)
{
    try
    {
        processMonitorCustomFormat = sourceProcess->CustomLogFormat;
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(
                L"Instantiation of a ProcessMonitor object failed. %S", ex.what()
            ).c_str()
        );
    }
}

/// <summary>
/// Instantiate the EventMonitor
/// </summary>
/// <param name="eventChannels">The list of EventLog channels</param>
void CreateEventMonitor(
    std::vector<EventLogChannel>& eventChannels,
    bool eventMonMultiLine,
    bool eventMonStartAtOldestRecord,
    const std::wstring& logFormat,
    const std::wstring& eventCustomLogFormat)
{
    try
    {
        g_eventMon = make_unique<EventMonitor>(
            eventChannels,
            eventMonMultiLine,
            eventMonStartAtOldestRecord,
            logFormat,
            eventCustomLogFormat
        );
    }
    catch (std::exception& ex)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Instantiation of an EventMonitor object failed. %S", ex.what()).c_str()
        );
    }
    catch (...)
    {
        logWriter.TraceError(L"Instantiation of an EventMonitor object failed. Unknown error occurred.");
    }
}

/// <summary>
/// Instantiate the EtwMonitor
/// </summary>
/// <param name="etwProviders">The list of ETW providers</param>
void CreateEtwMonitor(
    std::vector<ETWProvider>& etwProviders,
    const std::wstring& logFormat,
    const std::wstring& etwCustomLogFormat)
{
    try
    {
        g_etwMon = make_unique<EtwMonitor>(etwProviders, logFormat, etwCustomLogFormat);
    }
    catch (...)
    {
        logWriter.TraceError(L"Invalid providers. Check them using 'logman query providers'");
    }
}

/// <summary>
/// Start the monitors by delegating to the helper functions based on log source type
/// </summary>
/// <param name="settings">The LoggerSettings object containing configuration</param>
void StartMonitors(_In_ LoggerSettings& settings)
{
    // Vectors to store the event log channels and ETW providers
    std::vector<EventLogChannel> eventChannels;
    std::vector<ETWProvider> etwProviders;

    bool eventMonMultiLine;
    bool eventMonStartAtOldestRecord;
    bool etwMonMultiLine;

    // Set the log format from settings
    logFormat = settings.LogFormat;

    // Custom log formats for the different sources
    std::wstring eventCustomLogFormat;
    std::wstring etwCustomLogFormat;
    std::wstring processCustomLogFormat;

    // Iterate through each log source defined in the settings
    for (auto source : settings.Sources)
    {
        switch (source->Type)
        {
        case LogSourceType::EventLog:
        {
            std::shared_ptr<SourceEventLog> sourceEventLog =
                std::reinterpret_pointer_cast<SourceEventLog>(source);
            InitializeEventLogMonitor(sourceEventLog, eventChannels, eventMonMultiLine, eventMonStartAtOldestRecord, eventCustomLogFormat);
            break;
        }
        case LogSourceType::File:
        {
            std::shared_ptr<SourceFile> sourceFile =
                std::reinterpret_pointer_cast<SourceFile>(source);
            InitializeFileMonitor(sourceFile);
            break;
        }
        case LogSourceType::ETW:
        {
            std::shared_ptr<SourceETW> sourceETW =
                std::reinterpret_pointer_cast<SourceETW>(source);
            InitializeEtwMonitor(sourceETW, etwProviders, etwMonMultiLine, etwCustomLogFormat);
            break;
        }
        case LogSourceType::Process:
        {
            std::shared_ptr<SourceProcess> sourceProcess =
                std::reinterpret_pointer_cast<SourceProcess>(source);
            InitializeProcessMonitor(sourceProcess);
            break;
        }
        }
    }

    // Create and start EventMonitor if there are event channels
    if (!eventChannels.empty())
    {
        CreateEventMonitor(eventChannels, eventMonMultiLine, eventMonStartAtOldestRecord, logFormat, eventCustomLogFormat);
    }

    // Create and start EtwMonitor if there are ETW providers
    if (!etwProviders.empty())
    {
        CreateEtwMonitor(etwProviders, logFormat, etwCustomLogFormat);
    }
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    std::wstring cmdline;
    PWCHAR configFileName = (PWCHAR)DEFAULT_CONFIG_FILENAME;
    int exitcode = 0;

    g_hStopEvent = CreateEvent(nullptr,            // default security attributes
                               TRUE,               // manual-reset event
                               FALSE,              // initial state is nonsignaled
                               nullptr);           // object name

    if (g_hStopEvent == NULL)
    {
        logWriter.TraceError(
            Utility::FormatString(L"Failed to create event. Error: %d", GetLastError()).c_str()
        );
        return 0;
    }

    //
    // Check if the option /Config was passed.
    //
    int indexCommandArgument = 1;

    if (argc == 2)
    {
        if ((_wcsnicmp(argv[1], ARGV_OPTION_HELP1, _countof(ARGV_OPTION_HELP1)) == 0) ||
            (_wcsnicmp(argv[1], ARGV_OPTION_HELP2, _countof(ARGV_OPTION_HELP2)) == 0))
        {
            PrintUsage();
            return 0;
        }
    }
    else if (argc >= 3)
    {
        if (_wcsnicmp(argv[1], ARGV_OPTION_CONFIG_FILE, _countof(ARGV_OPTION_CONFIG_FILE)) == 0)
        {
            configFileName = argv[2];
            indexCommandArgument = 3;
        }
    }

    LoggerSettings settings;
    //read the config file
    bool configFileReadSuccess = OpenConfigFile(configFileName, settings);

    //start the monitors
    if (configFileReadSuccess)
    {
        StartMonitors(settings);
    } else {
        logWriter.TraceError(L"Invalid configuration file.");
    }

    //
    // Set up a signal handler for common catastrophic failure signals
    //
    setSignalHandler(SIGSEGV, signalHandler); // Segmentation fault
    setSignalHandler(SIGFPE, signalHandler);  // Floating point exception
    setSignalHandler(SIGILL, signalHandler);  // Illegal instruction
    setSignalHandler(SIGABRT, signalHandler); // Abort

    //
    // Set the Ctrl handler function, that propagates the Ctrl events to the child process.
    //
    SetConsoleCtrlHandler(ControlHandle, TRUE);

    //
    // Create the child process.
    //
    if (argc > indexCommandArgument)
    {
        cmdline = argv[indexCommandArgument];

        for (int i = indexCommandArgument + 1; i < argc; i++)
        {
            cmdline += L" ";
            cmdline += argv[i];
        }

        exitcode = CreateAndMonitorProcess(cmdline, logFormat, processMonitorCustomFormat);
    }
    else
    {
        DWORD waitResult = WaitForSingleObjectEx(g_hStopEvent, INFINITE, TRUE);

        switch (waitResult)
        {
            case WAIT_OBJECT_0:
            case WAIT_IO_COMPLETION:
                break;

            default:
                logWriter.TraceError(
                    Utility::FormatString(L"Log monitor wait failed. Error: %d", GetLastError()).c_str()
                );
                exitcode = 1;
                break;
        }
    }

    if (g_hStopEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = INVALID_HANDLE_VALUE;
    }

    return exitcode;
}
