#include "pch.h"

// Function to load and process the JSON file
bool loadAndProcessJson(_In_ const PWCHAR jsonFile, _Out_ LoggerSettings& Config) {
    // Convert PWCHAR to std::string
    std::wstring wstr(jsonFile);
    std::string jsonString = readJsonFromFile(wstr);

    boost::json::value parsedJson;
    try {
        parsedJson = boost::json::parse(jsonString);
        const boost::json::value& logConfig = parsedJson.at("LogConfig");
        processLogConfig(logConfig, Config);
    }
    catch (const boost::json::system_error& e) {
        logWriter.TraceError(L"Error parsing JSON.");
        return false;
    }

    if (parsedJson.is_null()) {
        logWriter.TraceError(L"Parsed JSON is null.");
        return false;
    }

    return true;
}

// Function to read a JSON file from a wstring path
std::string readJsonFromFile(_In_ const std::wstring& filePath) {
    // Open the file as a wide character stream
    std::wifstream wif(filePath);

    // Check if the file was opened successfully
    if (!wif.is_open()) {
        logWriter.TraceError(L"Failed to open JSON file.");
    }

    // Read the file into a wstring buffer
    std::wstringstream wss;
    wss << wif.rdbuf();

    // Close the file stream
    wif.close();

    // Convert wstring buffer to string (UTF-8) for further processing
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string jsonString = converter.to_bytes(wss.str());

    return jsonString;
}

// Function to handle EventLog type
void handleEventLog(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    bool startAtOldest = source.as_object().at("startAtOldestRecord").as_bool();
    bool multiLine = source.at("eventFormatMultiLine").as_bool();
    std::string customLogFormat = source.as_object().at("customLogFormat").as_string().c_str();

    Attributes[JSON_TAG_START_AT_OLDEST_RECORD] = reinterpret_cast<void*>(
        new std::wstring(startAtOldest ? L"true" : L"false")
        );
    Attributes[JSON_TAG_FORMAT_MULTILINE] = reinterpret_cast<void*>(new std::wstring(multiLine ? L"true" : L"false"));
    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        new std::wstring(Utility::string_to_wstring(customLogFormat))
        );

    // Process channels if they exist
    if (source.as_object().contains("channels")) {
        std::vector<EventLogChannel>* channels = new std::vector<EventLogChannel>();
        for (const auto& channel : source.as_object().at("channels").as_array()) {
            std::string name = channel.as_object().at("name").as_string().c_str();
            std::string levelString = channel.as_object().at("level").as_string().c_str();

            EventLogChannel channel(Utility::string_to_wstring(name), EventChannelLogLevel::Error);
            // Set the level based on the levelString
            if (channel.SetLevelByString(Utility::string_to_wstring(levelString))) {
                // Successfully set the level
            }
            else {
                logWriter.TraceError(
                    Utility::FormatString(
                        L"Invalid level string: %lu",
                        Utility::string_to_wstring(levelString)
                    ).c_str()
                );
            }
            // Add the channel to the vector
            channels->push_back(channel);
        }
        Attributes[JSON_TAG_CHANNELS] = channels;
    }

    std::shared_ptr<SourceEventLog> sourceEventLog = std::make_shared< SourceEventLog>();

    //
    // Fill the new EventLog source object, with its attributes
    //
    if (!SourceEventLog::Unwrap(Attributes, *sourceEventLog))
    {
        logWriter.TraceError(
            L"Error parsing configuration file. "
            L"Invalid EventLog source (it must have a non-empty 'channels')"
        );
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceEventLog)));
}

// Function to handle File type
void handleFile(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    std::string directory = source.as_object().at("directory").as_string().c_str();
    std::string filter = source.as_object().at("filter").as_string().c_str();
    bool includeSubdirs = source.at("includeSubdirectories").as_bool();
    std::string customLogFormat = source.as_object().at("customLogFormat").as_string().c_str();

    Attributes[JSON_TAG_DIRECTORY] = reinterpret_cast<void*>(new std::wstring(Utility::string_to_wstring(directory)));
    Attributes[JSON_TAG_FILTER] = reinterpret_cast<void*>(new std::wstring(Utility::string_to_wstring(filter)));
    Attributes[JSON_TAG_INCLUDE_SUBDIRECTORIES] = reinterpret_cast<void*>(
        new std::wstring(includeSubdirs ? L"true" : L"false")
        );
    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        new std::wstring(
            Utility::string_to_wstring(customLogFormat)
        )
        );

    std::shared_ptr<SourceFile> sourceFile = std::make_shared< SourceFile>();
    //
    // Fill the new FileLog source object, with its attributes
    //
    if (!SourceFile::Unwrap(Attributes, *sourceFile))
    {
        logWriter.TraceError(
            L"Error parsing configuration file. "
            L"Invalid File source (it must have a non-empty 'channels')"
        );
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceFile)));
}

// Function to handle ETW type
void handleETW(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    bool multiLine = source.at("eventFormatMultiLine").as_bool();
    std::string customLogFormat = source.at("customLogFormat").as_string().c_str();

    Attributes[JSON_TAG_FORMAT_MULTILINE] = reinterpret_cast<void*>(new std::wstring(multiLine ? L"true" : L"false"));
    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        new std::wstring(
            Utility::string_to_wstring(customLogFormat)
        )
        );

    std::vector<ETWProvider>* etwProviders = new std::vector<ETWProvider>();

    const boost::json::array& providers = source.at("providers").as_array();
    for (const auto& provider : providers) {
        std::string providerName = provider.as_object().at("providerName").as_string().c_str();
        std::string providerGuid = provider.as_object().at("providerGuid").as_string().c_str();
        std::string level = provider.as_object().at("level").as_string().c_str();

        ETWProvider etwProvider;
        etwProvider.ProviderName = Utility::string_to_wstring(providerName);
        etwProvider.SetProviderGuid(Utility::string_to_wstring(providerGuid));
        etwProvider.StringToLevel(Utility::string_to_wstring(level));

        // Check if "keywords" exists and process it if available
        auto keywordsIter = provider.as_object().find("keywords");
        if (keywordsIter != provider.as_object().end()) {
            std::string keywords = keywordsIter->value().as_string().c_str();
            etwProvider.Keywords = wcstoull(Utility::string_to_wstring(keywords).c_str(), NULL, 0);
        }

        etwProviders->push_back(etwProvider);
    }
    Attributes[JSON_TAG_PROVIDERS] = etwProviders;

    std::shared_ptr<SourceETW> sourceETW = std::make_shared< SourceETW>();
    //
    // Fill the new ETW source object, with its attributes
    //
    if (!SourceETW::Unwrap(Attributes, *sourceETW))
    {
        logWriter.TraceError(
            L"Error parsing configuration file. "
            L"Invalid ETW source (it must have a non-empty 'channels')"
        );
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceETW)));
}

// Function to handle Process type
void handleProcess(
    _In_ const boost::json::value& source,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource>>& Sources
) {
    std::string customLogFormat = source.at("customLogFormat").as_string().c_str();

    Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] = reinterpret_cast<void*>(
        new std::wstring(
            Utility::string_to_wstring(customLogFormat)
        )
        );

    std::shared_ptr<SourceProcess> sourceProcess = std::make_shared< SourceProcess>();
    //
    // Fill the new Process Log source object, with its attributes
    //
    if (!SourceProcess::Unwrap(Attributes, *sourceProcess))
    {
        logWriter.TraceError(L"Error parsing configuration file. Invalid Process source");
    }

    Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceProcess)));
}

void processLogConfig(const boost::json::value& logConfig, _Out_ LoggerSettings& Config) {
    if (logConfig.is_object()) {
        const boost::json::object& obj = logConfig.as_object();

        // Check if the "logFormat" field exists in LogConfig
        if (obj.contains("logFormat")) {
            std::string logFormat = obj.at("logFormat").as_string().c_str();
            Config.LogFormat = Utility::string_to_wstring(logFormat);
        }

        // Process the sources array
        if (obj.contains("sources") && obj.at("sources").is_array()) {
            const boost::json::array& sources = obj.at("sources").as_array();
            for (const auto& source : sources) {
                if (source.is_object()) {
                    const boost::json::object& srcObj = source.as_object();

                    std::string sourceType = srcObj.at("type").as_string().c_str();

                    AttributesMap sourceAttributes;

                    if (sourceType == "EventLog") {
                        handleEventLog(source, sourceAttributes, Config.Sources);
                    } else if (sourceType == "File") {
                        handleFile(source, sourceAttributes, Config.Sources);
                    } else if (sourceType == "ETW") {
                        handleETW(source, sourceAttributes, Config.Sources);
                    } else if (sourceType == "Process") {
                        handleProcess(source, sourceAttributes, Config.Sources);
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

// Clean up allocated memory (example cleanup function)
void cleanupAttributes(_In_ AttributesMap& Attributes) {
    for (auto attributePair : Attributes)
    {
        if (attributePair.second != nullptr)
        {
            delete attributePair.second;
        }
    }
}

