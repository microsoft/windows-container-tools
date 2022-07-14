//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#ifndef PCH_H
#define PCH_H

// C system
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <strsafe.h>
#include <time.h>

// C system, Windows/MS specific
#include <evntrace.h>
#include <in6addr.h>
#include <sal.h>
#include <shlwapi.h>
#include <tchar.h>
#include <tdh.h>
#include <wbemidl.h>
#include <Windows.h>
#include <winevt.h>
#include <winsock.h>
#include <wmistr.h>

// C++ system
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

// local
#include "./EtwMonitor.h"
#include "./EventMonitor.h"
#include "./LogFileMonitor.h"
#include "./LogWriter.h"
#include "./Parser/ConfigFileParser.h"
#include "./Parser/JsonFileParser.h"
#include "./Parser/LoggerSettings.h"
#include "./ProcessMonitor.h"
#include "./Utility.h"

#endif //PCH_H
