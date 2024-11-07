//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"

/// <summary>
/// Loads a JSON file, parses its contents, and processes its configuration settings.
/// </summary>
/// <param name="jsonFile">The path to the JSON file.</param>
/// <param name="Config">The LoggerSettings object to populate with the parsed configuration data.</param>
/// <returns>Returns true if the JSON file is successfully loaded and processed; otherwise,
/// returns false on error.</returns>
bool loadAndProcessJson(_In_ const PWCHAR jsonFile, _Out_ LoggerSettings& Config) {
    std::wstring wstr(jsonFile);
    std::string jsonString = readJsonFromFile(wstr);

    boost::json::value parsedJson;
    try {
        parsedJson = boost::json::parse(jsonString);
        const boost::json::value& logConfig = parsedJson.at("LogConfig");
        processLogConfig(logConfig, Config);
    }
    catch (const boost::system::system_error& e) {
        // Handle JSON-specific parsing errors
        logWriter.TraceError(
            Utility::FormatString(
                L"Error parsing JSON: %S",
                e.what()
            ).c_str()
        );
        return false;
    }
    catch (const std::exception& e) {
        // Handle other standard exceptions
        logWriter.TraceError(
            Utility::FormatString(
                L"An unexpected error occurred: %S",
                e.what()
            ).c_str()
        );
        return false;
    }
    catch (...) {
        // Handle any unknown exceptions
        logWriter.TraceError(L"An unknown error occurred.");
        return false;
    }

    if (parsedJson.is_null()) {
        logWriter.TraceError(L"Parsed JSON is null.");
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
void handleEventLog(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    bool startAtOldest = source.as_object().at("startAtOldestRecord").as_bool();
    bool multiLine = source.at("eventFormatMultiLine").as_bool();
    std::string customLogFormat = source.as_object().at("customLogFormat").as_string().c_str();

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
    if (source.as_object().contains("channels")) {
        auto channels = new std::vector<EventLogChannel>();
        for (const auto& channel : source.as_object().at("channels").as_array()) {
            std::string name = getJsonStringCaseInsensitive(channel.as_object(), "name");
            std::string levelString = getJsonStringCaseInsensitive(channel.as_object(), "level");

            EventLogChannel eventChannel(Utility::string_to_wstring(name), EventChannelLogLevel::Error);

            // Set the level based on the levelString, logging an error if invalid
            if (!eventChannel.SetLevelByString(Utility::string_to_wstring(levelString))) {
                logWriter.TraceError(
                    Utility::FormatString(
                        L"Invalid level string: %S",
                        Utility::string_to_wstring(levelString).c_str()
                    ).c_str()
                );
            }

            channels->push_back(eventChannel); // Add to the vector
        }

        Attributes[JSON_TAG_CHANNELS] = reinterpret_cast<void*>(channels);
    }
    else {
        Attributes[JSON_TAG_CHANNELS] = nullptr;
    }

    auto sourceEventLog = std::make_shared<SourceEventLog>();
    if (!SourceEventLog::Unwrap(Attributes, *sourceEventLog)) {
        logWriter.TraceError(L"Error parsing configuration file. Invalid EventLog source");
        return;
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceEventLog)));
}

/// <summary>
/// Parses and processes configuration data specific to File type logs, initializing a SourceFile object.
/// </summary>
/// <param name="source">JSON value containing configuration data.</param>
/// <param name="Attributes">Map of attributes for storing configuration details.</param>
/// <param name="Sources">Vector of log sources.</param>
void handleFileLog(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    std::string directory = getJsonStringCaseInsensitive(source.as_object(), "directory");
    std::string filter = getJsonStringCaseInsensitive(source.as_object(), "filter");
    bool includeSubdirs = source.at("includeSubdirectories").as_bool();
    std::string customLogFormat = getJsonStringCaseInsensitive(source.as_object(), "customLogFormat");

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
        return;
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceFile)));
}

/// <summary>
/// Parses and processes configuration details specific to ETW logs, initializing a SourceETW object.
/// </summary>
/// <param name="source">JSON value containing configuration data.</param>
/// <param name="Attributes">Map of attributes for storing configuration details.</param>
/// <param name="Sources">Vector of log sources.</param>
void handleETWLog(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    bool multiLine = source.at("eventFormatMultiLine").as_bool();
    std::string customLogFormat = source.at("customLogFormat").as_string().c_str();

    // Store multiLine and customLogFormat as wide strings in Attributes.
    Attributes[JSON_TAG_FORMAT_MULTILINE] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(multiLine ? L"true" : L"false").release()
        );
    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(Utility::string_to_wstring(customLogFormat)).release()
        );

    std::vector<ETWProvider> etwProviders;

    const boost::json::array& providers = source.at("providers").as_array();
    for (const auto& provider : providers) {
        std::string providerName = getJsonStringCaseInsensitive(provider.as_object(), "providerName");
        std::string providerGuid = getJsonStringCaseInsensitive(provider.as_object(), "providerGuid");
        std::string level = getJsonStringCaseInsensitive(provider.as_object(), "level");

        ETWProvider etwProvider;
        etwProvider.ProviderName = Utility::string_to_wstring(providerName);
        etwProvider.SetProviderGuid(Utility::string_to_wstring(providerGuid));
        etwProvider.StringToLevel(Utility::string_to_wstring(level));

        // Check if "keywords" exists and process it if available.
        auto keywordsIter = provider.as_object().find("keywords");
        if (keywordsIter != provider.as_object().end()) {
            std::string keywords = keywordsIter->value().as_string().c_str();
            etwProvider.Keywords = wcstoull(Utility::string_to_wstring(keywords).c_str(), NULL, 0);
        }

        etwProviders.push_back(etwProvider);
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
        return;
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceETW)));
}

/// <summary>
/// Parses and processes configuration details specific to Process logs, initializing a SourceProcess object.
/// </summary>
/// <param name="source">JSON value with configuration data.</param>
/// <param name="Attributes">Map of attributes for storing configuration details.</param>
/// <param name="Sources">Vector of log sources.</param>
void handleProcessLog(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    std::string customLogFormat = source.at("customLogFormat").as_string().c_str();

    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        std::make_unique<std::wstring>(Utility::string_to_wstring(customLogFormat)).release()
        );

    auto sourceProcess = std::make_shared<SourceProcess>();
    if (!SourceProcess::Unwrap(Attributes, *sourceProcess)) {
        logWriter.TraceError(L"Error parsing configuration file. Invalid Process source");
        return;
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceProcess)));
}

/// <summary>
/// Processes the logging configuration from a JSON object, populating the LoggerSettings structure.
/// </summary>
/// <param name="logConfig">JSON value containing logging configuration details.</param>
/// <param name="Config">LoggerSettings structure to populate with the parsed configuration.</param>
void processLogConfig(const boost::json::value& logConfig, _Out_ LoggerSettings& Config) {
    if (logConfig.is_object()) {
        const boost::json::object& obj = logConfig.as_object();

        // Check if the "logFormat" field exists in LogConfig
        std::string logFormat = getJsonStringCaseInsensitive(obj, "logFormat");
        if (!logFormat.empty()) {
            Config.LogFormat = Utility::string_to_wstring(logFormat);
        }

        // Process the sources array
        if (obj.contains("sources") && obj.at("sources").is_array()) {
            const boost::json::array& sources = obj.at("sources").as_array();
            for (const auto& source : sources) {
                if (source.is_object()) {
                    const boost::json::object& srcObj = source.as_object();

                    std::string sourceType = getJsonStringCaseInsensitive(srcObj, "type");

                    AttributesMap sourceAttributes;

                    if (sourceType == "EventLog") {
                        handleEventLog(source, sourceAttributes, Config.Sources);
                    } else if (sourceType == "File") {
                        handleFileLog(source, sourceAttributes, Config.Sources);
                    } else if (sourceType == "ETW") {
                        handleETWLog(source, sourceAttributes, Config.Sources);
                    } else if (sourceType == "Process") {
                        handleProcessLog(source, sourceAttributes, Config.Sources);
					} else {
						logWriter.TraceError(
							Utility::FormatString(
								L"Invalid source type: %S",
								Utility::string_to_wstring(sourceType).c_str()
							).c_str()
						);
					}

                    cleanupAttributes(sourceAttributes);
                }
            }
        }
        else {
            logWriter.TraceError(L"Sources array not found or invalid in LogConfig.");
        }
    }
    else {
        logWriter.TraceError(L"Invalid LogConfig object.");
    }
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
std::string getJsonStringCaseInsensitive(_In_ const boost::json::object& obj, _In_ const std::string& key) {
    auto it = std::find_if(obj.begin(), obj.end(),
        [&](const auto& item) {
            std::string currentKey = std::string(item.key().data());
            std::transform(currentKey.begin(), currentKey.end(), currentKey.begin(), ::tolower);
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            return currentKey == lowerKey;
        });

    if (it != obj.end() && it->value().is_string()) {
        return std::string(it->value().as_string().data());
    }

    logWriter.TraceError(
        Utility::FormatString(
            L"Key %S not found in JSON object", key.c_str()
        ).c_str()
    );

    // Return an empty string if the key is not found or the value is not a string.
    return "";
}

