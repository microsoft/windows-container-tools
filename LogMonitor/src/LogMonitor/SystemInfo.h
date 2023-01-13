//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#include "pch.h"


class SystemInfo {
private:
    SYSTEM_INFO systemInfo;
 public:

    LPCWSTR Platform;
    LPCWSTR NetBIOS;
    LPCWSTR DnsHostname;
    LPCWSTR DnsDomain;
    LPCWSTR DnsFullyQualified;
    LPCWSTR PhysicalNetBIOS;
    LPCWSTR PhysicalDnsHostname;
    LPCWSTR PhysicalDnsDomain;
    LPCWSTR PhysicalDnsFullyQualified;

    bool IsAmd64();
    bool IsX86();
    bool isARM();
    bool ARM64();
  
    bool IsWinXPOrAbove();
    bool IsWin7OrAbove();
    bool IsWinVistaOrAbove();
    bool IsWin8OrAbove();
    bool IsWin10OrAbove();
    bool ISWindowsServer();
	
    SystemInfo() {

    GetSystemInfo(&this->systemInfo);

    TCHAR buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD dwSize = _countof(buffer);
    int cnf = 0;

    for (cnf = 0; cnf < ComputerNameMax; cnf++) {
    if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)cnf, buffer, &dwSize)) {
        // log/handle the error here as well
        return;
    } else {
        switch (cnf) {
            case 0: this->NetBIOS = buffer;
              break;
            case 1: this->DnsHostname = buffer;
              break;
            case 2: this->DnsDomain = buffer;
               break;
            case 3: this->DnsFullyQualified = buffer;
              break;
            case 4: this->PhysicalNetBIOS = buffer;
              break;
            case 5: this->PhysicalDnsHostname = buffer;
              break;
            case 6: this->PhysicalDnsDomain = buffer;
              break;
            case 7: this->PhysicalDnsFullyQualified = buffer;
              break;
            default:
              break;
        }
    }

    dwSize = _countof(buffer);
    ZeroMemory(buffer, dwSize);
    }
    }

    ~SystemInfo() {}
};
