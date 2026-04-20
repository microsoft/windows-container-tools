//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#define DEFAULT_CONFIG_FILENAME L"C:\\LogMonitor\\LogMonitorConfig.json"

#define JSON_TAG_LOG_CONFIG L"LogConfig"
#define JSON_TAG_SOURCES L"sources"

///
/// Log formatting attributes
///
#define JSON_TAG_LOG_FORMAT L"logFormat"
#define JSON_TAG_CUSTOM_LOG_FORMAT L"customLogFormat"

///
/// Valid source attributes
///
#define JSON_TAG_TYPE L"type"
#define JSON_TAG_FORMAT_MULTILINE L"eventFormatMultiLine"
#define JSON_TAG_START_AT_OLDEST_RECORD L"startAtOldestRecord"
#define JSON_TAG_CHANNELS L"channels"
#define JSON_TAG_DIRECTORY L"directory"
#define JSON_TAG_FILTER L"filter"
#define JSON_TAG_INCLUDE_SUBDIRECTORIES L"includeSubdirectories"
#define JSON_TAG_PROVIDERS L"providers"
#define JSON_TAG_WAITINSECONDS L"waitInSeconds"

///
/// Valid channel attributes
///
#define JSON_TAG_CHANNEL_NAME L"name"
#define JSON_TAG_CHANNEL_LEVEL L"level"

///
/// Valid ETW provider attributes
///
#define JSON_TAG_PROVIDER_NAME L"providerName"
#define JSON_TAG_PROVIDER_GUID L"providerGuid"
#define JSON_TAG_PROVIDER_LEVEL L"level"
#define JSON_TAG_KEYWORDS L"keywords"

//
// Define the AttributesMap, that is a map<wstring, void*> with case
// insensitive keys
//

enum class EventChannelLogLevel
{
    Critical = 1,
    Error = 2,
    Warning = 3,
    Information = 4,
    Verbose = 5,
    All = 6,
};

DEFINE_ENUM_FLAG_OPERATORS(EventChannelLogLevel);

///
/// String names of the EventChannelLogLevel enum, used to parse the config file
///
const std::wstring LogLevelNames[] = {
    L"Critical",
    L"Error",
    L"Warning",
    L"Information",
    L"Verbose"
};

///
/// Array with EventChannelLogLevel enum values, used to get the real value,
/// when you have the index of the desired value at LogLevelNames.
///
const EventChannelLogLevel LogLevelValues[] = {
    EventChannelLogLevel::Critical,
    EventChannelLogLevel::Error,
    EventChannelLogLevel::Warning,
    EventChannelLogLevel::Information,
    EventChannelLogLevel::Verbose
};

inline
bool
StringToGuid(_In_ const std::wstring& str, _Out_ GUID& guid)
{
    std::wstring guidStr;

    //
    // Validate if is a GUID
    //
    if (str.length() == 36)
    {
        guidStr = str;
    }
    else if (str.length() == 38
        && str[0] == '{'
        && str[37] == '}')
    {
        guidStr = str.substr(1, 36);
    }
    else
    {
        return false;
    }

    for (int i = 0; i < 36; i++)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (guidStr[i] != '-')
            {
                return false;
            }
        }
        else if (!isxdigit(guidStr[i]))
        {
            return false;
        }
    }

    int count = swscanf_s(guidStr.c_str(),
        L"%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
        &guid.Data1, &guid.Data2, &guid.Data3,
        &guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
        &guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

    //
    // Check if all the data was successfully read
    //
    return count == 11;
}

enum class LogSourceType
{
    EventLog = 0,
    File,
    ETW,
    Process
};

///
/// String names of the LogSourceType enum, used to parse the config file
///
const LPCWSTR LogSourceTypeNames[] = {
    L"EventLog",
    L"File",
    L"ETW",
    L"Process"
};

///
/// Base class of a generic source configuration.
/// it only include the type (used to recover the real type with polymorphism)
///
class LogSource
{
public:
    LogSourceType Type;
};

///
/// Information about an event log channel, It includes its name and Log level
///
typedef struct _EventLogChannel
{
    std::wstring Name;
    EventChannelLogLevel Level = EventChannelLogLevel::Error;

    _EventLogChannel()
        : Name(L""), Level(EventChannelLogLevel::Error) {}

    _EventLogChannel(const std::wstring& name, EventChannelLogLevel level = EventChannelLogLevel::Error)
        : Name(name), Level(level) {}

    inline bool IsValid()
    {
        return !Name.empty();
    }

    inline bool SetLevelByString(
        _In_ const std::wstring& Str
    )
    {
        int errorLevelSize = sizeof(LogLevelNames) / sizeof(LogLevelNames[0]);

        for (int i = 0; i < errorLevelSize; i++)
        {
            if (_wcsicmp(Str.c_str(), LogLevelNames[i].c_str()) == 0)
            {
                Level = LogLevelValues[i];
                return true;
            }
        }

        return false;
    }
} EventLogChannel;

///
/// Represents a Source of EventLog type
///
class SourceEventLog : LogSource
{
public:
    std::vector<EventLogChannel> Channels;
    bool EventFormatMultiLine = true;
    bool StartAtOldestRecord = false;
    std::wstring CustomLogFormat = L"[%TimeStamp%] [%Source%] [%Severity%] %Message%";

    static bool Unwrap(
        _In_ AttributesMap& Attributes,
        _Out_ SourceEventLog& NewSource)
    {
        NewSource.Type = LogSourceType::EventLog;

        //
        // Get required 'channels' value
        //
        if (Attributes.find(JSON_TAG_CHANNELS) == Attributes.end()
            || Attributes[JSON_TAG_CHANNELS] == nullptr)
        {
            return false;
        }

        //
        // Clone the array, the original one could be deleted.
        //
        NewSource.Channels = *(std::vector<EventLogChannel>*)Attributes[JSON_TAG_CHANNELS];

        //
        // eventFormatMultiLine is an optional value
        //
        if (Attributes.find(JSON_TAG_FORMAT_MULTILINE) != Attributes.end()
            && Attributes[JSON_TAG_FORMAT_MULTILINE] != nullptr)
        {
            NewSource.EventFormatMultiLine = *(bool*)Attributes[JSON_TAG_FORMAT_MULTILINE];
        }

        //
        // startAtOldestRecord is an optional value
        //
        if (Attributes.find(JSON_TAG_START_AT_OLDEST_RECORD) != Attributes.end()
            && Attributes[JSON_TAG_START_AT_OLDEST_RECORD] != nullptr)
        {
            NewSource.StartAtOldestRecord = *(bool*)Attributes[JSON_TAG_START_AT_OLDEST_RECORD];
        }

        //
        // lineLogFormat is an optional value
        //
        if (Attributes.find(JSON_TAG_CUSTOM_LOG_FORMAT) != Attributes.end()
            && Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] != nullptr)
        {
            NewSource.CustomLogFormat = *(std::wstring*)Attributes[JSON_TAG_CUSTOM_LOG_FORMAT];
        }

        return true;
    }
};

///
/// Represents a Source of File type
///
class SourceFile : LogSource
{
public:
    std::wstring Directory;
    std::wstring Filter;
    bool IncludeSubdirectories = false;
    std::wstring CustomLogFormat = L"[%TimeStamp%] [%Source%] [%FileName%] %Message%";

    // Default wait time: 5minutes
    std::double_t WaitInSeconds = 300;

    static bool Unwrap(
        _In_ AttributesMap& Attributes,
        _Out_ SourceFile& NewSource)
    {
        NewSource.Type = LogSourceType::File;

        //
        // Directory is required
        //
        if (Attributes.find(JSON_TAG_DIRECTORY) == Attributes.end()
            || Attributes[JSON_TAG_DIRECTORY] == nullptr)
        {
            return false;
        }

        //
        // Clone the value, the original one could be deleted.
        //
        NewSource.Directory = *(std::wstring*)Attributes[JSON_TAG_DIRECTORY];

        //
        // Filter is an optional value
        //
        if (Attributes.find(JSON_TAG_FILTER) != Attributes.end()
            && Attributes[JSON_TAG_FILTER] != nullptr)
        {
            NewSource.Filter = *(std::wstring*)Attributes[JSON_TAG_FILTER];
        }

        //
        // includeSubdirectories is an optional value
        //
        if (Attributes.find(JSON_TAG_INCLUDE_SUBDIRECTORIES) != Attributes.end()
            && Attributes[JSON_TAG_INCLUDE_SUBDIRECTORIES] != nullptr)
        {
            NewSource.IncludeSubdirectories = *(bool*)Attributes[JSON_TAG_INCLUDE_SUBDIRECTORIES];
        }

        //
        // waitInSeconds is an optional value
        //
        if (Attributes.find(JSON_TAG_WAITINSECONDS) != Attributes.end()
            && Attributes[JSON_TAG_WAITINSECONDS] != nullptr)
        {
            NewSource.WaitInSeconds = *(std::double_t*)Attributes[JSON_TAG_WAITINSECONDS];
        }

        //
        // lineLogFormat is an optional value
        //
        if (Attributes.find(JSON_TAG_CUSTOM_LOG_FORMAT) != Attributes.end()
            && Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] != nullptr)
        {
            NewSource.CustomLogFormat = *(std::wstring*)Attributes[JSON_TAG_CUSTOM_LOG_FORMAT];
        }

        return true;
    }
};

///
/// ETW Provider
///
class ETWProvider
{
public:
    std::wstring ProviderName;
    std::wstring ProviderGuidStr;
    GUID ProviderGuid = { 0 };
    ULONGLONG Keywords = 0;
    UCHAR Level = 2; // Error level

    inline bool IsValid()
    {
        return !ProviderName.empty() || !ProviderGuidStr.empty();
    }

    inline bool SetProviderGuid(const std::wstring &value)
    {
        GUID guid;

        if (!StringToGuid(value, guid))
        {
            return false;
        }

        ProviderGuid = guid;
        ProviderGuidStr = value;

        return true;
    }

    inline bool StringToLevel(
        _In_ const std::wstring& Str
    )
    {
        int errorLevelSize = (sizeof(LogLevelNames) / sizeof(LogLevelNames[0])); // Don't include the ALL value

        for (UCHAR i = 0; i < errorLevelSize; i++)
        {
            if (_wcsicmp(Str.c_str(), LogLevelNames[i].c_str()) == 0)
            {
                //
                // Level starts at 1
                //
                Level = i + 1;
                return true;
            }
        }

        return false;
    }
};

///
/// Represents a Source if ETW type
///
class SourceETW : LogSource
{
public:
    std::vector<ETWProvider> Providers;
    bool EventFormatMultiLine = true;
    std::wstring CustomLogFormat = L"[%TimeStamp%] [%Source%] [%Severity%] "
        L"[%ProviderId%] [%ProviderName%] "
        L"[%EventId%] %EventData%";

    static bool Unwrap(
        _In_ AttributesMap& Attributes,
        _Out_ SourceETW& NewSource)
    {
        NewSource.Type = LogSourceType::ETW;

        //
        // Get required 'providers' value
        //
        if (Attributes.find(JSON_TAG_PROVIDERS) == Attributes.end()
            || Attributes[JSON_TAG_PROVIDERS] == nullptr)
        {
            return false;
        }

        //
        // Clone the array, the original one could be deleted.
        //
        NewSource.Providers = *(std::vector<ETWProvider>*)Attributes[JSON_TAG_PROVIDERS];

        //
        // eventFormatMultiLine is an optional value
        //
        if (Attributes.find(JSON_TAG_FORMAT_MULTILINE) != Attributes.end()
            && Attributes[JSON_TAG_FORMAT_MULTILINE] != nullptr)
        {
            NewSource.EventFormatMultiLine = *(bool*)Attributes[JSON_TAG_FORMAT_MULTILINE];
        }

        //
        // lineLogFormat is an optional value
        //
        if (Attributes.find(JSON_TAG_CUSTOM_LOG_FORMAT) != Attributes.end()
            && Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] != nullptr)
        {
            NewSource.CustomLogFormat = *(std::wstring*)Attributes[JSON_TAG_CUSTOM_LOG_FORMAT];
        }


        return true;
    }
};

///
/// Represents a Source if Proccess type
///
class SourceProcess : LogSource
{
    public:
        std::wstring CustomLogFormat = L"[%TimeStamp%] [%Source%] [%Message%]";

        static bool Unwrap(
            _In_ AttributesMap& Attributes,
            _Out_ SourceProcess& NewSource)
        {
            NewSource.Type = LogSourceType::Process;

            //
            // lineLogFormat is an optional value
            //
            if (Attributes.find(JSON_TAG_CUSTOM_LOG_FORMAT) != Attributes.end()
                && Attributes[JSON_TAG_CUSTOM_LOG_FORMAT] != nullptr)
            {
                NewSource.CustomLogFormat = *(std::wstring*)Attributes[JSON_TAG_CUSTOM_LOG_FORMAT];
            }

            return true;
        }
};

///
/// Information about a channel Log
///
typedef struct _LoggerSettings
{
    std::vector<std::shared_ptr<LogSource> > Sources;
    std::wstring LogFormat = L"JSON";
} LoggerSettings;
