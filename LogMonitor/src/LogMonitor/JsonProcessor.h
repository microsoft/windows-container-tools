//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

void handleEventLog(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

void handleFile(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

void handleETW(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

void handleProcess(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
);

bool loadAndProcessJson(
    _In_ const PWCHAR jsonFile,
    _Out_ LoggerSettings& Config
);

std::string readJsonFromFile(
    _In_ const std::wstring& filePath
);

void processLogConfig(
    const boost::json::value& logConfig,
    _Out_ LoggerSettings& Config
);

void cleanupAttributes(
    _In_ AttributesMap& Attributes
);
