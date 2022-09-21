//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//


#include "pch.h"

const DWORD BUFSIZE = 2048;
HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL; 


DWORD HandlePipeStream(HANDLE hPipe) {
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwRead = 0, dwWritten;
    char chBuf[BUFSIZE];
    bool bSuccess = false;

    while (1) {
        bSuccess = ReadFile(hPipe, chBuf, BUFSIZE * sizeof(char), &dwRead, NULL);

        DWORD readFileError = GetLastError();
        if (!bSuccess || dwRead == 0)
        {
            logWriter.WriteConsoleLog(std::to_wstring(readFileError));
            break;
        }

        bSuccess = logWriter.WriteLog(hParentStdOut,
                                    chBuf,
                                    dwRead,
                                    &dwWritten,
                                    NULL);

    }

    FlushFileBuffers(hPipe); 
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    return 1;
}

DWORD ConnectToClient(HANDLE hPipe)
{

    BOOL connected = FALSE;
    DWORD  dwThreadId = 0; 
    HANDLE hThread = NULL;



    connected = ConnectNamedPipe(hPipe, NULL) ? 
        TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 

    if(connected) {
        
        hThread = CreateThread( 
            NULL,
            0,
            HandlePipeStream,
            hPipe,
            0, 
            &dwThreadId
        );

        if (hThread != NULL) {
            CloseHandle(hThread);
        }

    } else {
        // failed to connect, close the pipe
        CloseHandle(hPipe);
    }
        

    return ERROR_SUCCESS;
}

HANDLE CreatePipe() {
    
    LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\logMonitor"); 

    return CreateNamedPipe(
        lpszPipename,
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,   
        PIPE_UNLIMITED_INSTANCES,
        512,
        0,
        5000,
        NULL
    );
}



/* Entry Point to Create and Run the Named Pipe
which allows any process to pipe its logs to LogMonitor in a manner similar to: ping 127.0.0.1 > \\.\pipe\logMonitor
The pipe name is currently hardcoded but we can build additional functionality allow users to specify the name of the pipe they prefer.

TODO:
- This only works with cmd not powershell, understand why and how to fix
- Improve error handling and logging
- Look into pipe access control and any security risks involved
- Should we create symlinks to the pipe?
- Improve code quality and docs
- Not tested on a container yet
*/
DWORD StartLogMonitorPipe() {
    while(1) {
        hPipe = CreatePipe();

        if (hPipe == INVALID_HANDLE_VALUE) 
        {
            logWriter.WriteConsoleLog(L"CreateNamedPipe Failed");
            return -1;
        }

        ConnectToClient(hPipe);
    }

    return ERROR_SUCCESS;
}