//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

#define BUFSIZE 4096
#define VARNAME TEXT("LOGMONITOR_TELEMETRY")

SystemInfo::SystemInfo() {
    DWORD dwRet, dwErr;
    LPTSTR pszOldVal;
    BOOL fExist;
    HKEY hKey;

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

    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    mHardwareInfo.dwOemId = siSysInfo.dwOemId;
    mHardwareInfo.dwNumberOfProcessors = siSysInfo.dwNumberOfProcessors;
    mHardwareInfo.dwPageSize = siSysInfo.dwPageSize;
    mHardwareInfo.dwProcessorType = siSysInfo.dwProcessorType;
    mHardwareInfo.lpMaximumApplicationAddress = siSysInfo.lpMaximumApplicationAddress;
    mHardwareInfo.lpMinimumApplicationAddress = siSysInfo.lpMinimumApplicationAddress;
    mHardwareInfo.wProcessorArchitecture = siSysInfo.wProcessorArchitecture;


    pszOldVal = (LPTSTR)malloc(BUFSIZE * sizeof(TCHAR));

    if (NULL == pszOldVal) {
        logWriter.TraceError(
            Utility::FormatString(L"Out of memory. %lu", GetLastError()).c_str());
    }

    dwRet = GetEnvironmentVariable(VARNAME, pszOldVal, BUFSIZE);

    if (0 == dwRet) {
        dwErr = GetLastError();
        if (ERROR_ENVVAR_NOT_FOUND == dwErr) {
            logWriter.TraceError(
            Utility::FormatString(L"Environment variable does not exist. %lu", GetLastError()).c_str());
            fExist = FALSE;
        }
    } else if (BUFSIZE < dwRet) {
        pszOldVal = (LPTSTR)realloc(pszOldVal, dwRet * sizeof(TCHAR));
        if (NULL == pszOldVal) {
            logWriter.TraceError(
            Utility::FormatString(L"Out of memory. %lu", GetLastError()).c_str());
        }
        dwRet = GetEnvironmentVariable(VARNAME, pszOldVal, dwRet);
        if (!dwRet) {
            logWriter.TraceError(
            Utility::FormatString(L"GetEnvironmentVariable failed %lu", GetLastError()).c_str());
        } else {
            fExist = TRUE;
        }
    } else {
        fExist = TRUE;
    }

    bool match = wcscmp(pszOldVal, L"0") == 0;
    if (fExist && match) {
        mEnableTelemetryReporting = false;
    }

    free(pszOldVal);
}

EnvVariable SystemInfo::GetEnvVars() {
    return mEnvironmentVariable;
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

bool SystemInfo::ISWindowsServer() {
    return IsWindowsServer();
}

SystemInfo::~SystemInfo() { }
