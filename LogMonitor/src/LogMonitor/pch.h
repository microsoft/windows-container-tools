//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
#ifndef PCH_H
#define PCH_H

#define NOMINMAX

// TODO(annandaa): reorder the header files as per convention.
// currently there are some implicit dependencies, things break when
// you try to reorder.

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
#include <VersionHelpers.h>
#include "Utility.h"
#include "Parser/ConfigFileParser.h"
#include "Parser/LoggerSettings.h"
#include "Parser/JsonFileParser.h"
#include "LogWriter.h"
#include "EtwMonitor.h"
#include "EventMonitor.h"
#include "FileMonitor/Utilities.h"
#include "LogFileMonitor.h"
#include "ProcessMonitor.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"

#endif //PCH_H
