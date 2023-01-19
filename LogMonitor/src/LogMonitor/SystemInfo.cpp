//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

SystemInfo::SystemInfo() {
    HKEY hKey;
    std::wstring strValueOfBinDir;


    LONG lRes = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE, REG_KEY_CUR_VER_STR, 0, KEY_READ, &hKey);
    switch (lRes) {
        case ERROR_SUCCESS:
         Utility::GetStringRegKey(
            hKey,
            BUILD_BRANCH_STR_VALUE_NAME,
            mRegistryCurrentVersion.BuildBranch,
            REG_KEY_STR_DEFAULT_VALUE);
         Utility::GetStringRegKey(
            hKey,
            BUILD_LAB_STR_VALUE_NAME,
            mRegistryCurrentVersion.BuildLab,
            REG_KEY_STR_DEFAULT_VALUE);
         Utility::GetStringRegKey(
            hKey,
            CURRENT_BUILD_NUMBER_STR_VALUE_NAME,
            mRegistryCurrentVersion.CurrentBuildNumber,
            REG_KEY_STR_DEFAULT_VALUE);
        Utility::GetStringRegKey(
            hKey,
            INSTALLATION_TYPE_STR_VALUE_NAME,
            mRegistryCurrentVersion.InstallationType,
            REG_KEY_STR_DEFAULT_VALUE);
        Utility::GetDWORDRegKey(
            hKey,
            CURR_MINOR_VER_NUM_STR_VALUE_NAME,
            mRegistryCurrentVersion.CurrentMinorVersionNumber,
            REG_KEY_DW_DEFAULT_VALUE);
        Utility::GetDWORDRegKey(
            hKey,
            CUR_MAJOR_VER_NUM_STR_VALUE_NAME,
            mRegistryCurrentVersion.CurrentMajorVersionNumber,
            REG_KEY_DW_DEFAULT_VALUE);
        Utility::GetStringRegKey(
            hKey,
            PRODUCT_NAME_STR_VALUE_NAME,
            mRegistryCurrentVersion.ProductName,
            REG_KEY_STR_DEFAULT_VALUE);
        break;
    case ERROR_FILE_NOT_FOUND:
        logWriter.TraceError(
            Utility::FormatString(L"Key not found").c_str());
        break;
    default:
        logWriter.TraceError(
            Utility::FormatString(L"Error opening key").c_str());
        break;
    }

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

RegistryCurrentVersion SystemInfo::GetRegCurVersion() {
    return mRegistryCurrentVersion;
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
