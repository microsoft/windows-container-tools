//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

const LPCWSTR REG_KEY_CUR_VER = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

const std::wstring BUILD_LAB = L"BuildLab";
const std::wstring BUILD_LAB_EX = L"BuildLabEx";
const std::wstring CURRENT_BUILD_NUMBER = L"CurrentBuildNumber";
const std::wstring INSTALLATION_TYPE = L"InstallationType";
const std::wstring DISPLAY_VERSION = L"DisplayVersion";
const std::wstring CURR_MINOR_VER_NUM = L"CurrentMinorVersionNumber";
const std::wstring CUR_MAJOR_VER_NUM = L"CurrentMajorVersionNumber";
const std::wstring PRODUCT_NAME = L"ProductName";

const std::wstring REG_KEY_STR_DEFAULT_VALUE;
const DWORD REG_KEY_DW_DEFAULT_VALUE = 0;
const bool REG_KEY_BOOL_DEFAULT_VALUE = false;
struct EnvVariable
{
  LPTSTR LOGMONITOR_TELEMETRY;
};

struct HardwareInformation
{
  DWORD dwOemId;
  WORD wProcessorArchitecture;
  DWORD dwNumberOfProcessors;
  DWORD dwPageSize;
  DWORD dwProcessorType;
  LPVOID lpMinimumApplicationAddress;
  LPVOID lpMaximumApplicationAddress;
};

struct RegistryCurrentVersion
{
  std::wstring BuildLab;
  std::wstring BuildLabEx;
  std::wstring CurrentBuildNumber;
  std::wstring InstallationType;
  std::wstring DisplayVersion;
  DWORD CurrentMinorVersionNumber;
  DWORD CurrentMajorVersionNumber;
  std::wstring ProductName;
};



class SystemInfo {

 private:
    EnvVariable mEnvironmentVariable;
    HardwareInformation mHardwareInfo;
    RegistryCurrentVersion mRegistryCurrentVersion;
    bool mEnableTelemetryReporting = true;

 public:
  SystemInfo();
  EnvVariable GetEnvVars();
  HardwareInformation GetHardInfo();
  RegistryCurrentVersion GetRegCurVersion();
  bool GetTelemetryFlag();
  ~SystemInfo();
};
