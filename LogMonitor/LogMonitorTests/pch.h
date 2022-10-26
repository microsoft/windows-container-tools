//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here

#include "CppUnitTest.h"
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
#include <regex>
#include <stdexcept>
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
#include <codecvt>
#include "shlwapi.h"
#include <direct.h >
#include <io.h> 
#include <fcntl.h> 
#include "../src/LogMonitor/Utility.h"
#include "../src/LogMonitor/Parser/ConfigFileParser.h"
#include "../src/LogMonitor/Parser/LoggerSettings.h"
#include "../src/LogMonitor/Parser/JsonFileParser.h"
#include "../src/LogMonitor/LogWriter.h"
#include "../src/LogMonitor/EtwMonitor.h"
#include "../src/LogMonitor/EventMonitor.h"
#include "../src/LogMonitor/FileMonitor/Utilities.h"
#include "../src/LogMonitor/LogFileMonitor.h"
#include "../src/LogMonitor/ProcessMonitor.h"
#include "Utility.h"
#endif //PCH_H
