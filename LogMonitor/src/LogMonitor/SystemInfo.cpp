//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

SystemInfo::SystemInfo() {
    TCHAR buffer[256] = TEXT("");
    DWORD dwSize = _countof(buffer);
    int cnf = 0;
    for (cnf = 0; cnf < ComputerNameMax; cnf++) {
        if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)cnf, buffer, &dwSize)) {
            logWriter.TraceError(
                Utility::FormatString(L"GetComputerNameEx failed %lu", GetLastError()).c_str());
            return;
        } else {
            switch (cnf) {
            case 0: mCompName.NetBIOS = buffer;
                break;
            case 1: mCompName.DnsHostname = buffer;
                 break;
            case 2: mCompName.DnsDomain = buffer;
                 break;
            case 3: mCompName.DnsFullyQualified = buffer;
                 break;
            case 4: mCompName.PhysicalNetBIOS = buffer;
                 break;
            case 5: mCompName.PhysicalDnsHostname = buffer;
                 break;
            case 6: mCompName.PhysicalDnsDomain = buffer;
                 break;
            case 7: mCompName.PhysicalDnsFullyQualified = buffer;
                 break;
            default:
                break;
            }
        }
        dwSize = _countof(buffer);
        ZeroMemory(buffer, dwSize);
    }

    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    mHardwareInfo.dwOemId = siSysInfo.dwOemId;
    mHardwareInfo.dwNumberOfProcessors = siSysInfo.dwNumberOfProcessors;
    mHardwareInfo.dwPageSize = siSysInfo.dwPageSize;
    mHardwareInfo.dwProcessorType = siSysInfo.dwProcessorType;
    mHardwareInfo.lpMaximumApplicationAddress = siSysInfo.lpMaximumApplicationAddress;
    mHardwareInfo.lpMinimumApplicationAddress = siSysInfo.lpMinimumApplicationAddress;
    mHardwareInfo.wProcessorArchitecture = siSysInfo.wProcessorArchitecture;


    LPTSTR lpszVariable;
    LPTCH lpvEnv;
    lpvEnv = GetEnvironmentStrings();

    if (lpvEnv == NULL) {
        logWriter.TraceError(
            Utility::FormatString(L"GetEnvironmentStrings failed %lu", GetLastError()).c_str());
    }

    lpszVariable = (LPTSTR) lpvEnv;

    while (*lpszVariable) {
        lpszVariable += lstrlen(lpszVariable) + 1;
        if (wcscmp(lpszVariable, L"LOGMONITOR_TELEMETRY=0") == 0) {
            mEnableTelemetryReporting = false;
        }
    }
    FreeEnvironmentStrings(lpvEnv);
}

EnvVariable SystemInfo::GetEnvVars() {
    return mEnvironmentVariable;
}

ComputerName SystemInfo::GetCompName() {
    return mCompName;
}

HardwareInformation SystemInfo::GetHardInfo() {
    return mHardwareInfo;
}

bool SystemInfo::GetTelemetryFlag() {
    return mEnableTelemetryReporting;
}

bool SystemInfo::IsWinXPOrAbove() {
    return IsWindowsXPOrGreater();
}

bool SystemInfo::IsWin7OrAbove() {
    return IsWindows7OrGreater();
}

bool SystemInfo::IsWinVistaOrAbove() {
    return IsWindowsVistaOrGreater();
}

bool SystemInfo::IsWin8OrAbove() {
    return IsWindows8OrGreater();
}

bool SystemInfo::IsWin10OrAbove() {
    return IsWindows10OrGreater();
}

bool SystemInfo::ISWindowsServer() {
    return IsWindowsServer();
}

SystemInfo::~SystemInfo() { }
