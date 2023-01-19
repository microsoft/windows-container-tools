//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

const LPCWSTR REG_KEY_CUR_VER_STR = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

const std::wstring BUILD_BRANCH_STR_VALUE_NAME = L"BuildBranch";
const std::wstring BUILD_LAB_STR_VALUE_NAME = L"BuildLab";
const std::wstring CURRENT_BUILD_NUMBER_STR_VALUE_NAME = L"CurrentBuildNumber";
const std::wstring INSTALLATION_TYPE_STR_VALUE_NAME = L"InstallationType";
const std::wstring CURR_MINOR_VER_NUM_STR_VALUE_NAME = L"CurrentMinorVersionNumber";
const std::wstring CUR_MAJOR_VER_NUM_STR_VALUE_NAME = L"CurrentMajorVersionNumber";
const std::wstring PRODUCT_NAME_STR_VALUE_NAME = L"ProductName";

const std::wstring REG_KEY_STR_DEFAULT_VALUE;
const DWORD REG_KEY_DW_DEFAULT_VALUE = 0;
const bool REG_KEY_BOOL_DEFAULT_VALUE = false;
struct EnvVariable
{
  LPTSTR LOGMONITOR_TELEMETRY;
  // TODO(bosira) WE CAN ADD MORE ENVIRONMENT VARIABLE VALUES THAT WE WANT TO REPORT
};

struct ComputerName
{
    LPCWSTR Platform;
    LPCWSTR NetBIOS;
    LPCWSTR DnsHostname;
    LPCWSTR DnsDomain;
    LPCWSTR DnsFullyQualified;
    LPCWSTR PhysicalNetBIOS;
    LPCWSTR PhysicalDnsHostname;
    LPCWSTR PhysicalDnsDomain;
    LPCWSTR PhysicalDnsFullyQualified;
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
  std::wstring BuildBranch;
  std::wstring BuildLab;
  std::wstring CurrentBuildNumber;
  std::wstring InstallationType;
  DWORD CurrentMinorVersionNumber;
  DWORD CurrentMajorVersionNumber;
  std::wstring ProductName;
};



class SystemInfo {

 private:
    EnvVariable mEnvironmentVariable;
    ComputerName mCompName;
    HardwareInformation mHardwareInfo;
    RegistryCurrentVersion mRegistryCurrentVersion;
    bool mEnableTelemetryReporting = true;

 public:
  SystemInfo();
  EnvVariable GetEnvVars();
  ComputerName GetCompName();
  HardwareInformation GetHardInfo();
  RegistryCurrentVersion GetRegCurVersion();
  bool GetTelemetryFlag();
  bool IsWinXPOrAbove();
  bool IsWin7OrAbove();
  bool IsWinVistaOrAbove();
  bool IsWin8OrAbove();
  bool IsWin10OrAbove();
  bool ISWindowsServer();
  ~SystemInfo();
};
