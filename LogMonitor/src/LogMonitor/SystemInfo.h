//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

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


class SystemInfo {

 private:
    EnvVariable mEnvironmentVariable;
    ComputerName mCompName;
    HardwareInformation mHardwareInfo;
    bool mEnableTelemetryReporting = true;

 public:
  SystemInfo();
  EnvVariable GetEnvVars();
  ComputerName GetCompName();
  HardwareInformation GetHardInfo();
  bool GetTelemetryFlag();
  bool IsWinXPOrAbove();
  bool IsWin7OrAbove();
  bool IsWinVistaOrAbove();
  bool IsWin8OrAbove();
  bool IsWin10OrAbove();
  bool ISWindowsServer();
  ~SystemInfo();
};
