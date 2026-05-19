//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
#ifndef LOGMONITOR_SRC_LOGMONITOR_PCH_H_
#define LOGMONITOR_SRC_LOGMONITOR_PCH_H_

// TODO(annandaa): reorder the header files as per convention.
// currently there are some implicit dependencies, things break when
// you try to reorder.

// NOLINTBEGIN(build/include_order)
#include <functional>
#include <conio.h>
#include <stdio.h>
#include <array>
#include <memory>
#include <variant>
#include <cstdint>
#include <string>
#include <algorithm>
#include <sstream>
#include <vector>
#include <queue>
#include <map>
#include <stdexcept>
#include <csignal>
#include <cstdlib>
#include <Windows.h>
#include <cctype>
#include <sal.h>
#include <winevt.h>
#include <wbemidl.h>
#include <wmistr.h>
#include <evntrace.h>
#include <tdh.h>
#include <in6addr.h>
#include <winsock.h>
#include <time.h>
#include <iostream>
#include <tchar.h>
#include <strsafe.h>
#include <fstream>
#include <streambuf>
#include <system_error>
#include <locale>
#include <codecvt>
#include "shlwapi.h"
#include <io.h>
#include <fcntl.h>
// NOLINTEND(build/include_order)
#include <nlohmann/json.hpp>
#include "Utility.h"  // NOLINT(build/include_subdir)
#include "Parser/ConfigFileParser.h"  // NOLINT(build/include_subdir)
#include "Parser/LoggerSettings.h"  // NOLINT(build/include_subdir)
#include "Parser/JsonFileParser.h"  // NOLINT(build/include_subdir)
#include "LogWriter.h"  // NOLINT(build/include_subdir)
#include "EtwMonitor.h"  // NOLINT(build/include_subdir)
#include "EventMonitor.h"  // NOLINT(build/include_subdir)
#include "FileMonitor/FileMonitorUtilities.h"  // NOLINT(build/include_subdir)
#include "LogFileMonitor.h"  // NOLINT(build/include_subdir)
#include "ProcessMonitor.h"  // NOLINT(build/include_subdir)
#include "JsonProcessor.h"  // NOLINT(build/include_subdir)

#endif  // LOGMONITOR_SRC_LOGMONITOR_PCH_H_
