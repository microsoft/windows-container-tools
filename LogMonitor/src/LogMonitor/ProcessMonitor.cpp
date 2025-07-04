//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

using namespace std;

#define BUFSIZE 4096

HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

DWORD g_processId = 0;
wstring g_processName = L"";

wstring loggingformat, processCustomLogFormat;


ProcessMonitor::ProcessMonitor(){}

///
/// Creates a new process, and link its STDIN and STDOUT to the LogMonitor proccess' ones.
///
/// \param Cmdline      The command to start the new process.
///
/// \return Status
///
DWORD CreateAndMonitorProcess(std::wstring& Cmdline, std::wstring LogFormat, std::wstring ProcessCustomLogFormat)
{
    loggingformat = LogFormat;
    processCustomLogFormat = ProcessCustomLogFormat;

    SECURITY_ATTRIBUTES saAttr;
    DWORD status = ERROR_SUCCESS;

    //
    // Set the bInheritHandle flag so pipe handles are inherited.
    //
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    //
    // Create a pipe for the child process's STDOUT.
    //
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
    {
        status = GetLastError();

        logWriter.TraceError(
            Utility::FormatString(
                L"Process monitor error. Failed to create stdout pipe. Error: %lu",
                status
            ).c_str()
        );

        return status;
    }

    //
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    //
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
    {
        status = GetLastError();

        logWriter.TraceError(
            Utility::FormatString(
                L"Process monitor error. Failed to update handle to stdout pipe. Error: %lu",
                status
            ).c_str()
        );

        return status;
    }

    //
    // Create the child process.
    //

    return CreateChildProcess(Cmdline);
}

///
/// Create a child process that uses the previously created pipe for STDOUT.
///
/// \param Cmdline      The command to start the new process.
///
DWORD CreateChildProcess(std::wstring& Cmdline)
{
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    bool bSuccess = false;
    DWORD exitcode = 0;
    wchar_t cmdl[32768]; // The maximum size of CreateProcess's lpCommandLine parameter is 32,768

    ZeroMemory(&cmdl, sizeof(cmdl));
    wcscpy_s(cmdl, 32768, Cmdline.c_str());

    //
    // Set up members of the PROCESS_INFORMATION structure.
    //
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    //
    // Set up members of the STARTUPINFO structure.
    // This structure specifies the STDIN and STDOUT handles for redirection.
    //
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr; // redirect errors too to avoid log interleaving
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    //
    // Create the child process.
    //
    bSuccess = CreateProcess(NULL,
                             cmdl,          // command line
                             NULL,          // process security attributes
                             NULL,          // primary thread security attributes
                             TRUE,          // handles are inherited
                             0,             // creation flags
                             NULL,          // use parent's environment
                             NULL,          // use parent's current directory
                             &siStartInfo,  // STARTUPINFO pointer
                             &piProcInfo);  // receives PROCESS_INFORMATION

    g_processId = piProcInfo.dwProcessId;
    g_processName = Cmdline;

//
// If an error occurs, exit the application.
//
    if (!bSuccess)
    {
        exitcode = GetLastError();

        logWriter.TraceError(
            Utility::FormatString(L"Failed to start entrypoint process. Error: %lu", exitcode).c_str()
        );
    }
    else
    {
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ReadFromPipe, NULL, 0, NULL);
        WaitForSingleObject(piProcInfo.hProcess, INFINITE);

        if (GetExitCodeProcess(piProcInfo.hProcess, &exitcode))
        {
            logWriter.TraceInfo(
                Utility::FormatString(L"Entrypoint processs exit code: %d", exitcode).c_str()
            );
        }
        else
        {
            logWriter.TraceError(
                Utility::FormatString(
                    L"Process monitor error. Failed to get entrypoint process exit code. Error: %d",
                    GetLastError()
                ).c_str()
            );
        }

        //
        // Close handles to the child process and its primary thread.
        //
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
    }

    return exitcode;
}

///
/// Helper function for making a copy of the buffer.
/// returns the index after the last copied byte.
/// 
size_t BufferCopy(char* dst, char* src, size_t start, size_t end = 0)
{
    char* ptr = src;
    size_t i = start;
    while (*ptr > 0 && i < BUFSIZE) {
        // *ptr > 0: leave out the '\0' for the const char*
        if (i == end && end > 0) {
            break;
        }
        dst[i++] = *ptr;
        ptr++;
    }

    return i;
}

///
/// Helper function for making a copy of the buffer.
/// For optimization, the function also "sanitizes" the string for JSON.
/// returns the index after the last copied byte.
/// 
size_t BufferCopyAndSanitize(char* dst, char* src)
{
    char* ptr = src;
    size_t i = 0;
    while (*ptr > 0 && i < BUFSIZE) {
        // *ptr > 0: leave out the '\0' for the const char*

        // clean, eg. replace \r\n with \\r\\n (displayed as "\r\n")
        if (*ptr == '\r') {
            dst[i++] = '\\';
            dst[i++] = 'r';
        }
        else if (*ptr == '\n') {
            dst[i++] = '\\';
            dst[i++] = 'n';
        }
        else if (*ptr == '\"') {
            dst[i++] = '\\';
            dst[i++] = '\"';
        }
        else if (*ptr == '\\' && i < BUFSIZE - 1 && *(ptr + 1) != '\\') {
            dst[i++] = '\\';
            dst[i++] = '\\';
        }
        else {
            dst[i++] = *ptr;
        }
        ptr++;
    }

    return i;
}

///
/// Helper function to format the stdout buffer to include additional
/// details from the JSON schema.
/// Returns the number of bytes written to the buffer.
///
std::string FormatProcessLog(const std::string& line) {
    if (Utility::CompareWStrings(loggingformat, L"Custom")) {
        return FormatCustomLog(line);
    } else {
        return FormatStandardLog(line);
    }
}

///
/// Helper function to format the custom log.
///
std::string FormatCustomLog(const std::string& inputLine) {
    ProcessLogEntry logEntry;
    SYSTEMTIME st;
    GetSystemTime(&st);

    logEntry.source = L"Process";
    logEntry.currentTime = Utility::SystemTimeToString(st).c_str();

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> fromBytesConverter;
    logEntry.message = fromBytesConverter.from_bytes(inputLine);

    std::wstring formattedLog = Utility::FormatEventLineLog(processCustomLogFormat, &logEntry, logEntry.source);

    std::wstring_convert<std::codecvt_utf8<wchar_t>> toBytesConverter;
    std::string logLine = toBytesConverter.to_bytes(formattedLog) + "\n";

    return logLine;
}

///
/// Helper function to format the standard log (JSON or XML).
///
std::string FormatStandardLog(const std::string& inputLine) {
    std::string prefix, suffix;

    if (Utility::CompareWStrings(loggingformat, L"XML")) {
        prefix = "<Log><Source>Process</Source><LogEntry><Logline>";
        suffix = "</Logline></LogEntry></Log>\n";
    } else {
        prefix = "{\"Source\":\"Process\",\"LogEntry\":{\"Logline\":\"";
        suffix = "\"},\"SchemaVersion\":\"1.0.0\"}\n";
    }

    // Sanitize the log line
    std::string sanitized;
    sanitized.reserve(inputLine.size());
    for (char c : inputLine) {
        sanitized += (c > 0) ? c : '?';
    }

    std::string logLine = prefix + sanitized + suffix;

    return logLine;
}


/// 
/// Helper function to clear the stdout buffer
/// return number of bytes cleared
/// 
size_t ClearBuffer(char* chBuf) {
    size_t count = 0;
    char* ptr = chBuf;

    while (count < BUFSIZE) {
        *ptr = 0; // null char
        ptr++;
        count++;
    }

    return count;
}

///
/// Read output from the child process's pipe for STDOUT
/// and write to the parent process's pipe for STDOUT.
/// Stop when there is no more data.
///
/// \param Param        UNUSED.
///
DWORD ReadFromPipe(LPVOID Param)
{
    DWORD dwWritten;
    char chBuf[BUFSIZE + 1] = { 0 };
    std::string partialLine;
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    UNREFERENCED_PARAMETER(Param);

    for (;;)
    {
        DWORD bytesRead = 0;
        BOOL bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &bytesRead, NULL);

        if (!bSuccess || bytesRead == 0)
            break;

        chBuf[bytesRead] = '\0'; // Null-terminate buffer
        partialLine.append(chBuf);

        size_t start = 0;
        size_t newlinePos;
        while ((newlinePos = partialLine.find_first_of("\r\n", start)) != std::string::npos)
        {
            std::string line = partialLine.substr(start, newlinePos - start);

            std::string formatted = FormatProcessLog(line);
            logWriter.WriteLog(
                hParentStdOut,
                formatted.c_str(),
                static_cast<DWORD>(formatted.size()),
                &dwWritten,
                NULL);

            // Skip over newline chars
            start = newlinePos + 1;
            while (start < partialLine.size() &&
                (partialLine[start] == '\r' || partialLine[start] == '\n'))
                ++start;
        }

        // Keep unprocessed remainder
        if (start < partialLine.size())
            partialLine = partialLine.substr(start);
        else
            partialLine.clear();
    }

    // Write remaining partial line
    if (!partialLine.empty())
    {
        std::string formatted = FormatProcessLog(partialLine);
        logWriter.WriteLog(
            hParentStdOut,
            formatted.c_str(),
            static_cast<DWORD>(formatted.size()),
            &dwWritten,
            NULL);
    }

    return ERROR_SUCCESS;
}

std::wstring ProcessMonitor::ProcessFieldsMapping(_In_ std::wstring fileFields, _In_ void* pLogEntryData)
{
    std::wostringstream oss;
    ProcessLogEntry* pLogEntry = (ProcessLogEntry*)pLogEntryData;

    if (Utility::CompareWStrings(fileFields, L"TimeStamp")) oss << pLogEntry->currentTime;
    if (Utility::CompareWStrings(fileFields, L"Source")) oss << pLogEntry->source;
    if (Utility::CompareWStrings(fileFields, L"Message")) oss << pLogEntry->message;

    return oss.str();
}
