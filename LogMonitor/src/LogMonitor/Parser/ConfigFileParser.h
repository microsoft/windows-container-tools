//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#include "JsonFileParser.h"
#include "LoggerSettings.h"
#include <map>
#include <vector>
#include <array>
#include <memory>
#include <variant>

bool OpenConfigFile(
    _In_ const PWCHAR ConfigFileName,
    _In_ LoggerSettings& Config
);

bool ReadConfigFile(
    _In_ JsonFileParser& Parser,
    _Out_ LoggerSettings& Config
);

bool ReadLogConfigObject(
    _In_ JsonFileParser& Parser,
    _Out_ LoggerSettings& Config
);

bool ReadSourceAttributes(
    _In_ JsonFileParser& Parser,
    _Out_ AttributesMap& Attributes
);

bool ReadLogChannel(
    _In_ JsonFileParser& Parser,
    _Out_ EventLogChannel& Result
);

bool ReadETWProvider(
    _In_ JsonFileParser& Parser,
    _Out_ ETWProvider& Result
);

bool AddNewSource(
    _In_ JsonFileParser& Parser,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource> >& Sources
);

void _PrintSettings(_Out_ LoggerSettings& Config);
