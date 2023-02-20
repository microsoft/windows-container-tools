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


DWORD CreateChildProcess(std::wstring& Cmdline);
DWORD ReadFromPipe(LPVOID Param);

///
/// Creates a new process, and link its STDIN and STDOUT to the LogMonitor proccess' ones.
///
/// \param Cmdline      The command to start the new process.
///
/// \return Status
///
DWORD CreateAndMonitorProcess(std::wstring& Cmdline)
{
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
size_t bufferCopy(char* dst, char* src, size_t start, size_t end = 0)
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
size_t bufferCopyAndSanitize(char* dst, char* src)
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
/// Helper function to formats the stdout buffer to include the other
/// details from the JSON schema.
/// Returns the number of bytes written to the buffer.
///
size_t formatProcessLog(char* chBuf)
{
    LoggerSettings settings;
    const char* prefix;
    const char* suffix;
    if (Utility::CompareWStrings(settings.LogFormat, L"JSON"))
    {
        // {"LogEntry":{"Source":"Process","Logline":"<chBuf>"},"SchemaVersion":"1.0.0"}
        prefix = "{\"LogEntry\":{\"Source\":\"Process\",\"Logline\":\"";
        suffix = "\"},\"SchemaVersion\":\"1.0.0\"}\n";
    }
    else {
        // <LogEntry><Source>Process</Source><Logline><chBuf>Z</Logline></LogEntry>
        prefix = "<LogEntry><Source>Process</Source><Logline>";
        suffix = "</Logline></LogEntry>\n";
    }
    
    char chBufCpy[BUFSIZE] = "";

    //
    // copy valid (>0 ASCII values) bytes from chBuf to chBufCpy
    //
    size_t chBufLen = bufferCopyAndSanitize(chBufCpy, chBuf);
    size_t prefixLen = strlen(prefix);
    size_t suffixLen = strlen(suffix);
    size_t index = bufferCopy(chBuf, const_cast<char*>(prefix), 0, prefixLen);

    // copy over the logline after prefix
    // index increments from the previous index within bufferCopy
    index = bufferCopy(chBuf, chBufCpy, index);

    // truncate, in the unlikely event of a long logline > |BUFSIZE-85|
    // leave at least 36 slots to close the JSON/XML
    // reset the start index
    if ((index + suffixLen) > BUFSIZE - 5) {
        index = BUFSIZE - 5 - suffixLen;
        if (Utility::CompareWStrings(settings.LogFormat, L"JSON"))
        {
            suffix = "...\"},\"SchemaVersion\":\"1.0.0\"}\n";
        } else {
            suffix = "...\</Logline></LogEntry>\n";
        }
    }

    index = bufferCopy(chBuf, const_cast<char*>(suffix), index, index + suffixLen);

    return index; // same as the number of bytes read
}

/// 
/// Helper function to clear the stdout buffer
/// return number of bytes cleared
/// 
size_t clearBuffer(char* chBuf) {
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
    DWORD dwRead, dwWritten;
    char chBuf[BUFSIZE] = "";
    char chBufOut[BUFSIZE] = "";
    char chBufRem[BUFSIZE] = ""; // for remaining characters
    bool bSuccess = false;
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    UNREFERENCED_PARAMETER(Param);

    for (;;)
    {
        // clear buffer ready for read
        clearBuffer(chBuf);
        // move valid chars from remainder buffer to chBuf
        // then ReadFile starts from the end position (chBuf + cnt)
        char* ptrRem = chBufRem;
        size_t cnt = 0;
        while (*ptrRem > 0 && cnt < BUFSIZE) {
            chBuf[cnt++] = *ptrRem;
            ptrRem++;
        }

        bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf + cnt, BUFSIZE - cnt, &dwRead, NULL);
        // clear remainder buffer for the next read
        clearBuffer(chBufRem);

        if (!bSuccess || dwRead == 0)
        {
            break;
        }

        // write out a line at a time
        char* ptr = chBuf;
        size_t outSz = 0;
        size_t count = 0;
        size_t lastNewline = 0;
        clearBuffer(chBufOut);
        while (*ptr > 0 && count < BUFSIZE) {
            // copy over to chBufOut till \r\n
            // the remaining will be reserved to be completed
            // by the next read.
            if (*ptr == '\r' || *ptr == '\n') {
                if (outSz > 0) {
                    // print out and reset chBufOut and outSz
                    size_t sz = formatProcessLog(chBufOut);
                    DWORD dwRead = static_cast<DWORD>(sz);

                    bSuccess = logWriter.WriteLog(
                        hParentStdOut,
                        chBufOut,
                        dwRead,
                        &dwWritten,
                        NULL);
                    // reset
                    outSz = 0;
                    clearBuffer(chBufOut);
                    lastNewline = count;
                }
            }
            else {
                chBufOut[outSz++] = *ptr;
            }
            ptr++;
            count++;
        }

        // move remaining characters to chBufRem
        ptrRem = chBuf;
        ptrRem += lastNewline;
        count = 0;
        while (*ptrRem > 0 && count < BUFSIZE) {
            chBufRem[count++] = *ptrRem;
            ptrRem++;
        }
    }

    return ERROR_SUCCESS;
}
