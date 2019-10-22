//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <iostream>
#include <tchar.h>
#include <strsafe.h>
#include <fstream>
#include <streambuf>
#include "ProcessMonitor.h"
#include "LogWriter.h"

using namespace std;

#define BUFSIZE 4096 
 
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

void CreateChildProcess(std::wstring& Cmdline); 
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
            Utility::FormatString(L"Stdout CreatePipe failed with %lu", status).c_str()
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
            Utility::FormatString(L"Stdout SetHandleInformation failed with %lu", status).c_str()
        );

        return status;
    }

    //
    // Create the child process. 
    //
    CreateChildProcess(Cmdline);

    return 0; 
} 

///
/// Create a child process that uses the previously created pipe for STDOUT.
///
/// \param Cmdline      The command to start the new process.
///
void CreateChildProcess(std::wstring& Cmdline)
{
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    bool bSuccess = false;
    DWORD status;
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
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);;
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

    //
    // If an error occurs, exit the application.
    //
    if (!bSuccess)
    {
        status = GetLastError();

        logWriter.TraceError(
            Utility::FormatString(L"CreateProcess failed with %lu", status).c_str()
        );
    }
    else 
    {
        DWORD exitcode;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ReadFromPipe, NULL, 0, NULL);
        WaitForSingleObject(piProcInfo.hProcess, INFINITE);

        if(GetExitCodeProcess(piProcInfo.hProcess, &exitcode))
        {
            logWriter.TraceInfo(
                Utility::FormatString(L"Processs exit code: %d", exitcode).c_str()
            );
        }
        else
        {
            logWriter.TraceError(
                Utility::FormatString(L"GetExitCodeProcess failed: %d", GetLastError()).c_str()
            );
        }

        //
        // Close handles to the child process and its primary thread.
        //
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
    }
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
    char chBuf[BUFSIZE]; 
    bool bSuccess = false;
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    UNREFERENCED_PARAMETER(Param);

    for (;;) 
    { 
        bSuccess = ReadFile( g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);

        if (!bSuccess || dwRead == 0)
        {
            break; 
        }

        bSuccess = logWriter.WriteLog(hParentStdOut,
                                      chBuf, 
                                      dwRead,
                                      &dwWritten,
                                      NULL);
        if (!bSuccess)
        {
            break;
        }
    }

    return ERROR_SUCCESS;
}
