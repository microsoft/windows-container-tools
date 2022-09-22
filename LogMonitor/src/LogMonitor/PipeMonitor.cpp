//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//


#include "pch.h"

const DWORD BUFSIZE = 2048;


DWORD HandlePipeStream(HANDLE hPipe) {
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwRead = 0, dwWritten;
    char chBuf[BUFSIZE];
    bool bSuccess = false;

    // Loop until done reading
    while (1) {
        // Read client requests from the pipe. only allows messages up to BUFSIZE characters in length.
        bSuccess = ReadFile(
            hPipe,      // handle to pipe 
            chBuf,      // buffer to receive data
            BUFSIZE * sizeof(char), // size of buffer
            &dwRead,                // number of bytes read
            NULL);                  // not overlapped I/O 

        DWORD readFileError = GetLastError();
        if (!bSuccess || dwRead == 0)
        {
            switch (readFileError)
            {
            case ERROR_BROKEN_PIPE:
                logWriter.TraceError(
                    Utility::FormatString(L"Client disconnected. Error: %lu", readFileError).c_str()
                );
                break;
            case ERROR_MORE_DATA:
                logWriter.TraceError(
                    Utility::FormatString(L"The next message is longer than number of bytes parameter specifies to read. Error: %lu", readFileError).c_str()
                );
                break;
                //more cases can be handled here apart from these two
            default:
                logWriter.TraceError(
                    Utility::FormatString(L"Another issue caused readFile failed. Error: %lu", readFileError).c_str()
                );
                logWriter.WriteConsoleLog(std::to_wstring(readFileError));
                break;
            }
            
            break;
        }

        bSuccess = logWriter.WriteLog(hParentStdOut,
                                    chBuf,
                                    dwRead,
                                    &dwWritten,
                                    NULL);

    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 

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
        
    // Create a thread for this client. 
    if(connected) {
        
        hThread = CreateThread( 
            NULL,
            0,
            HandlePipeStream,
            hPipe,
            0, 
            &dwThreadId
        );

        if (hThread == NULL) {
            throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateThread");
            return -1;
        } else {
            CloseHandle(hThread);
        }

    } else {
        // The client could not connect, close the pipe
        CloseHandle(hPipe);
    }
        

    return ERROR_SUCCESS;
}

HANDLE CreatePipe() {
    
    LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\logMonitor"); 

    return CreateNamedPipe(
        lpszPipename,           // pipe name
        PIPE_ACCESS_INBOUND,    // read/write access
        PIPE_TYPE_MESSAGE |    // message type pipe
        PIPE_READMODE_MESSAGE | // message-read mode
        PIPE_WAIT,              // blocking mode
        PIPE_UNLIMITED_INSTANCES, // max. instances
        512,                        // output buffer size
        0,                          // input buffer size
        5000,                       // client time-out
        NULL                        // default security attribute
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
    HANDLE hPipe = INVALID_HANDLE_VALUE;
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