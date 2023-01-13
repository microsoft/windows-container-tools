//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "SystemInfo.h"

bool SystemInfo::IsAmd64() {
    return this->systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
}

bool SystemInfo::IsX86() {
    return this->systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL;
}

bool SystemInfo::isARM() {
    return this->systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM;
}

bool SystemInfo::ARM64() {
    return this->systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64;
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
