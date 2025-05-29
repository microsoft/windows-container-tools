//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

#ifdef _WIN32
#include <string.h>
#define strcasecmp _stricmp
#endif

using json = nlohmann::json;

/// <summary>
/// Loads a JSON file, parses its contents, and processes its configuration settings.
/// </summary>
/// <param name="jsonFile">The path to the JSON file.</param>
/// <param name="Config">The LoggerSettings object to populate with the parsed configuration data.</param>
/// <returns>Returns true if the JSON file is successfully loaded and processed; otherwise,
/// returns false on error.</returns>
bool ReadConfigFile(_In_ const PWCHAR jsonFile, _Out_ LoggerSettings& Config) {
    std::wstring wstr(jsonFile);
    std::string jsonString = readJsonFromFile(wstr);

    try {
        json parsedJson = json::parse(jsonString);

        if (!parsedJson.contains("LogConfig")) {
            logWriter.TraceError(L"Missing 'LogConfig' section in JSON.");
            return false;
        }

        const auto& logConfig = parsedJson["LogConfig"];

        if (!processLogConfig(logConfig, Config)) {
            logWriter.TraceError(L"Failed to fully process LogConfig.");
            return false;
        }
    }
    catch (const json::parse_error& e) {
        logWriter.TraceError(
            Utility::FormatString(
                L"Error parsing JSON: %S",
                e.what()
            ).c_str()
        );
        return false;
    }
    catch (const std::exception& e) {
        logWriter.TraceError(
            Utility::FormatString(
                L"An unexpected error occurred: %S",
                e.what()
            ).c_str()
        );
        return false;
    }
    catch (...) {
        logWriter.TraceError(L"An unknown error occurred.");
        return false;
    }

    return true;
}

/// <summary>
/// Reads a JSON file from a given file path and returns its contents as a UTF-8 encoded string.
/// </summary>
/// <param name="filePath">The path to the JSON file.</param>
/// <returns>A UTF-8 encoded string containing the JSON file's contents, 
/// or an empty string if the file could not be opened.</returns>
std::string readJsonFromFile(_In_ const std::wstring& filePath) {
    // Open the file as a wide character input stream
    std::wifstream wif(filePath);
    if (!wif.is_open()) {
        logWriter.TraceError(L"Failed to open JSON file.");
        return "";
	}

    // Read the file into a wide string buffer
    std::wstringstream wss;
    wss << wif.rdbuf();	
    wif.close();

    // Convert the wstring buffer to a UTF-8 string
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string jsonString = converter.to_bytes(wss.str());

    return jsonString;
}

/// <summary>
/// Parses and processes configuration data for an EventLog log source, initializing a SourceEventLog object.
/// </summary>
/// <param name="source">JSON configuration data.</param>
/// <param name="Attributes">Map of attributes used to store the configuration details.</param>
/// <param name="Sources">Vector of log sources.</param>
/// <returns>
/// Returns true if the Event log source is successfully parsed and added to Sources;
/// otherwise, returns false if parsing fails.
/// </returns>
bool handleEventLog(
    _In_ const json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    try {
        bool startAtOldest = false;
        if (source.contains("startAtOldestRecord") && source["startAtOldestRecord"].is_boolean()) {
            startAtOldest = source["startAtOldestRecord"].get<bool>();
        }

        bool multiLine = false;
        if (source.contains("eventFormatMultiLine") && source["eventFormatMultiLine"].is_boolean()) {
            multiLine = source["eventFormatMultiLine"].get<bool>();
        }

        std::string customLogFormat;
        if (source.contains("customLogFormat") && source["customLogFormat"].is_string()) {
            customLogFormat = source["customLogFormat"].get<std::string>();
        }

        Attributes[JSON_TAG_START_AT_OLDEST_RECORD] = reinterpret_cast<void*>(
            std::make_unique<std::wstring>(startAtOldest ? L"true" : L"false").release()
            );

        Attributes[JSON_TAG_FORMAT_MULTILINE] = reinterpret_cast<void*>(
            std::make_unique<std::wstring>(multiLine ? L"true" : L"false").release()
            );

        Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
            std::make_unique<std::wstring>(Utility::string_to_wstring(customLogFormat)).release()
            );

        // Process channels if they exist
        if (source.contains("channels") && source["channels"].is_array()) {
            auto channels = new std::vector<EventLogChannel>();

            for (const auto& channel : source["channels"]) {
                std::string name = getJsonStringCaseInsensitive(channel, "name");
                std::string levelString = getJsonStringCaseInsensitive(channel, "level");

                EventLogChannel eventChannel(Utility::string_to_wstring(name), EventChannelLogLevel::Error);

                std::wstring level = Utility::string_to_wstring(levelString);
                // Set the level based on the levelString, logging an error if invalid
                if (!eventChannel.SetLevelByString(level)) {
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Invalid level string: %S",
                            levelString.c_str()
                        ).c_str()
                    );
                    delete channels; // Prevent leak
                    return false;
                }

                channels->push_back(eventChannel);
            }

            Attributes[JSON_TAG_CHANNELS] = reinterpret_cast<void*>(channels);
        }
        else {
            Attributes[JSON_TAG_CHANNELS] = nullptr;
        }

        auto sourceEventLog = std::make_shared<SourceEventLog>();
        if (!SourceEventLog::Unwrap(Attributes, *sourceEventLog)) {
            logWriter.TraceError(L"Error parsing configuration file. Invalid EventLog source");
            return false;
        }

        Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceEventLog)));
        return true;
        }
        catch (const std::exception& e) {
            logWriter.TraceError(
                Utility::FormatString(
                    L"Error parsing EventLog source: %S",
                    e.what()
                ).c_str()
            );
            return false;
        }
}

/// <summary>
/// Parses and processes configuration data specific to File type logs, initializing a SourceFile object.
/// </summary>
/// <param name="source">JSON value containing configuration data.</param>
/// <param name="Attributes">Map of attributes for storing configuration details.</param>
/// <param name="Sources">Vector of log sources.</param>
/// <returns>
/// Returns true if the File log source is successfully parsed and added to Sources;
/// otherwise, returns false if parsing fails.
/// </returns>
bool handleFileLog(
    _In_ const json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    std::string directory = getJsonStringCaseInsensitive(source, "directory");
    std::string filter = getJsonStringCaseInsensitive(source, "filter");
    bool includeSubdirs = false;
    if (source.contains("includeSubdirectories") && source["includeSubdirectories"].is_boolean()) {
        includeSubdirs = source["includeSubdirectories"].get<bool>();
    }
    std::string customLogFormat = getJsonStringCaseInsensitive(source, "customLogFormat");

    Attributes[JSON_TAG_DIRECTORY] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(Utility::string_to_wstring(directory)).release()
        );
    Attributes[JSON_TAG_FILTER] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(Utility::string_to_wstring(filter)).release()
        );
    Attributes[JSON_TAG_INCLUDE_SUBDIRECTORIES] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(includeSubdirs ? L"true" : L"false").release()
        );
    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(Utility::string_to_wstring(customLogFormat)).release()
        );

    auto sourceFile = std::make_shared<SourceFile>();
    if (!SourceFile::Unwrap(Attributes, *sourceFile)) {
        logWriter.TraceError(L"Error parsing configuration file. Invalid File source");
        return false;
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceFile)));
    return true;
}

/// <summary>
/// Parses and processes configuration details specific to ETW logs, initializing a SourceETW object.
/// </summary>
/// <param name="source">JSON value containing configuration data.</param>
/// <param name="Attributes">Map of attributes for storing configuration details.</param>
/// <param name="Sources">Vector of log sources.</param>
/// <returns>
/// Returns true if the ETW log source is successfully parsed and added to Sources;
/// otherwise, returns false if parsing fails.
/// </returns>
bool handleETWLog(
    _In_ const nlohmann::json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    bool multiLine = false;
    if (source.contains("eventFormatMultiLine") && source["eventFormatMultiLine"].is_boolean()) {
        multiLine = source["eventFormatMultiLine"].get<bool>();
    }

    std::string customLogFormat;
    if (source.contains("customLogFormat") && source["customLogFormat"].is_string()) {
        customLogFormat = source["customLogFormat"].get<std::string>();
    }

    // Store multiLine and customLogFormat as wide strings in Attributes
    Attributes[JSON_TAG_FORMAT_MULTILINE] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(multiLine ? L"true" : L"false").release()
        );
    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(Utility::string_to_wstring(customLogFormat)).release()
        );

    std::vector<ETWProvider> etwProviders;

    if (source.contains("providers") && source["providers"].is_array()) {
        for (const auto& provider : source["providers"]) {
            std::string providerName = getJsonStringCaseInsensitive(provider, "providerName");
            std::string providerGuid = getJsonStringCaseInsensitive(provider, "providerGuid");
            std::string level = getJsonStringCaseInsensitive(provider, "level");

            ETWProvider etwProvider;
            etwProvider.ProviderName = Utility::string_to_wstring(providerName);
            etwProvider.SetProviderGuid(Utility::string_to_wstring(providerGuid));
            etwProvider.StringToLevel(Utility::string_to_wstring(level));

            // Check if "keywords" exists and process it
            if (provider.contains("keywords") && provider["keywords"].is_string()) {
                std::string keywords = provider["keywords"].get<std::string>();
                etwProvider.Keywords = wcstoull(Utility::string_to_wstring(keywords).c_str(), nullptr, 0);
            }

            etwProviders.push_back(std::move(etwProvider));
        }
    }

    // Store the ETW providers in Attributes.
    Attributes[JSON_TAG_PROVIDERS] = reinterpret_cast<void*>(
        std::make_unique<std::vector<ETWProvider>>(std::move(etwProviders)).release()
        );

    auto sourceETW = std::make_shared<SourceETW>();
    if (!SourceETW::Unwrap(Attributes, *sourceETW)) {
        logWriter.TraceError(
            L"Error parsing configuration file. "
            L"Invalid ETW source (it must have a non-empty 'channels')"
        );
        return false;
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceETW)));

    return true;
}

/// <summary>
/// Parses and processes configuration details specific to Process logs, initializing a SourceProcess object.
/// </summary>
/// <param name="source">JSON value with configuration data.</param>
/// <param name="Attributes">Map of attributes for storing configuration details.</param>
/// <param name="Sources">Vector of log sources.</param>
/// <returns>
/// Returns true if the process log source is successfully parsed and added to Sources;
/// otherwise, returns false if parsing fails.
/// </returns>
bool handleProcessLog(
    _In_ const nlohmann::json& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    std::string customLogFormat;
    if (source.contains("customLogFormat") && source["customLogFormat"].is_string()) {
        customLogFormat = source["customLogFormat"].get<std::string>();
    }

    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(Utility::string_to_wstring(customLogFormat)).release()
        );

    auto sourceProcess = std::make_shared<SourceProcess>();
    if (!SourceProcess::Unwrap(Attributes, *sourceProcess)) {
        logWriter.TraceError(L"Error parsing configuration file. Invalid Process source");
        return false;
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceProcess)));

    return true;
}

/// <summary>
/// Processes the logging configuration from a JSON object, populating the LoggerSettings structure.
/// </summary>
/// <param name="logConfig">JSON value containing logging configuration details.</param>
/// <param name="Config">LoggerSettings structure to populate with the parsed configuration.</param>
/// <returns>
/// Returns true if the log configuration is valid and sources are successfully processed;
/// otherwise, returns false if the configuration is invalid or sources fail to process.
/// </returns>
bool processLogConfig(_In_ const nlohmann::json& logConfig, _Out_ LoggerSettings& Config) {
    if (!logConfig.is_object()) {
        logWriter.TraceError(L"Invalid LogConfig object.");
        return false;
    }

    const auto& obj = logConfig;

    std::string logFormat;
    if (obj.contains("logFormat") && obj["logFormat"].is_string()) {
        logFormat = obj["logFormat"].get<std::string>();
    }

    if (!logFormat.empty()) {
        Config.LogFormat = Utility::string_to_wstring(logFormat);
    } else {
        logWriter.TraceError(L"LogFormat not found in LogConfig. Using default log format.");
    }

    if (!obj.contains("sources") || !obj["sources"].is_array()) {
        logWriter.TraceError(L"Sources array not found or invalid in LogConfig.");
        return false;
    }

    // Process the sources array
    const auto& sources = obj["sources"];
    return processSources(sources, Config);
}

/// <summary>
/// Iterates through the sources array from the configuration, 
/// parsing and processing each log source based on its type.
/// </summary>
/// <param name="sources">JSON array containing different log sources.</param>
/// <param name="Config">LoggerSettings structure where parsed sources are stored.</param>
/// <returns>
/// Returns true if all sources are successfully processed;
/// otherwise, returns false if any source fails to process.
/// </returns>
bool processSources(_In_ const nlohmann::json& sources, _Out_ LoggerSettings& Config) {
    bool overallSuccess = true;

    if (!sources.is_array()) {
        logWriter.TraceError(L"Sources is not an array.");
        return false;
    }

    for (const auto& source : sources) {
        if (!source.is_object()) {
            logWriter.TraceError(L"Skipping invalid source entry (not an object).");
            overallSuccess = false;
            continue;
        }

        const auto& srcObj = source;

        std::string sourceType;
        if (srcObj.contains("type") && srcObj["type"].is_string()) {
            sourceType = srcObj["type"].get<std::string>();
        }

        if (sourceType.empty()) {
            logWriter.TraceError(L"Skipping source with missing or empty type.");
            overallSuccess = false;
            continue;
        }

        AttributesMap sourceAttributes;
        bool parseSuccess = false;

        if (sourceType == "EventLog") {
            parseSuccess = handleEventLog(source, sourceAttributes, Config.Sources);
        } else if (sourceType == "File") {
            parseSuccess = handleFileLog(source, sourceAttributes, Config.Sources);
        } else if (sourceType == "ETW") {
            parseSuccess = handleETWLog(source, sourceAttributes, Config.Sources);
        } else if (sourceType == "Process") {
            parseSuccess = handleProcessLog(source, sourceAttributes, Config.Sources);
        } else {
            logWriter.TraceError(
                Utility::FormatString(
                    L"Invalid source type: %S",
                    Utility::string_to_wstring(sourceType).c_str()
                ).c_str()
            );
            overallSuccess = false;
            continue;
        }

        if (!parseSuccess) {
            logWriter.TraceError(
                Utility::FormatString(
                    L"Failed to process source of type: %S",
                    Utility::string_to_wstring(sourceType).c_str()
                ).c_str()
            );
            overallSuccess = false;
        }

        cleanupAttributes(sourceAttributes);
    }

    return overallSuccess;
}

/// <summary>
/// Cleans up dynamically allocated memory in the Attributes map by deleting each non-null attribute pointer.
/// </summary>
/// <param name="Attributes">A map of attribute keys to pointers. </param>
void cleanupAttributes(_In_ AttributesMap& Attributes) {
    for (auto attributePair : Attributes)
    {
        if (attributePair.second != nullptr)
        {
            delete attributePair.second;
        }
    }
}

/// <summary>
/// Retrieves a string value from a JSON object in a case-insensitive manner.
/// </summary>
/// <param name="obj">The JSON object to search for the key.</param>
/// <param name="key">The key to search for in the JSON object, case-insensitive.</param>
/// <returns>string value associated with the key if found </returns>
std::string getJsonStringCaseInsensitive(_In_ const nlohmann::json& obj, _In_ const std::string& key) {
    auto lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        std::string currentKey = it.key();
        std::transform(currentKey.begin(), currentKey.end(), currentKey.begin(), ::tolower);
        if (currentKey == lowerKey && it.value().is_string()) {
            return it.value().get<std::string>();
        }
    }

    logWriter.TraceError(
        Utility::FormatString(
            L"Key %S not found in JSON object", key.c_str()
        ).c_str()
    );

    return "";
}

