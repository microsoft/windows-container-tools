//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#include <Windows.h>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include "Utility.h"  // NOLINT(build/include_subdir)

class LogSource;
typedef struct _LoggerSettings LoggerSettings;

bool handleEventLog(
    _In_ const nlohmann::json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

bool handleFileLog(
    _In_ const nlohmann::json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

bool handleETWLog(
    _In_ const nlohmann::json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

bool handleProcessLog(
    _In_ const nlohmann::json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

bool ReadConfigFile(
    _In_ const PWCHAR jsonFile,
    _Out_ LoggerSettings& Config
);

std::string readJsonFromFile(
    _In_ const std::wstring& filePath
);

bool processLogConfig(
    const nlohmann::json& logConfig,
    _Out_ LoggerSettings& Config
);

bool processSources(
    _In_ const nlohmann::json& sources,
    _Out_ LoggerSettings& Config
);

void cleanupAttributes(
    _In_ AttributesMap& Attributes
);

const nlohmann::json* findJsonKeyCaseInsensitive(
    _In_ const nlohmann::json& obj,
    _In_ const std::string& key
);

std::string getJsonStringCaseInsensitive(
    _In_ const nlohmann::json& obj,
    _In_ const std::string& key,
    _In_ bool required = false
);
