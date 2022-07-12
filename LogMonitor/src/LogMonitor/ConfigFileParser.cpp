//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "Parser/ConfigFileParser.h"
#include "LogWriter.h"

/// ConfigFileParser.cpp
///
/// Reads the configuration file content (as a string), parsing it with a
/// JsonFileParser object previously created.
///
/// The main entry point in this file is ReadConfigFile.
///


///
/// Read the root JSON of the config file
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Config       Returns a LoggerSettings struct with the values specified
///     in the config file.
///
/// \return True if the configuration file was valid. Otherwise false
///
bool
ReadConfigFile(
    _In_ JsonFileParser& Parser,
    _Out_ LoggerSettings& Config
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        logWriter.TraceError(L"Failed to parse configuration file. Object expected at the file's root");
        return false;
    }

    bool containsLogConfigTag = false;

    if (Parser.BeginParseObject())
    {
        do
        {
            const std::wstring key(Parser.GetKey());

            if (_wcsnicmp(key.c_str(), JSON_TAG_LOG_CONFIG, _countof(JSON_TAG_LOG_CONFIG)) == 0)
            {
                containsLogConfigTag = ReadLogConfigObject(Parser, Config);
            }
            else
            {
                Parser.SkipValue();
            }
        } while (Parser.ParseNextObjectElement());
    }

    return containsLogConfigTag;
}

///
/// Read LogConfig tag, that contains the config of the sources
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Config       Returns a LoggerSettings struct with the values specified
///                     in the config file.
///
/// \return True if LogConfig tag was valid. Otherwise false
///
bool
ReadLogConfigObject(
    _In_ JsonFileParser& Parser,
    _Out_ LoggerSettings& Config
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        logWriter.TraceError(L"Failed to parse configuration file. 'LogConfig' is expected to be an object");
        Parser.SkipValue();
        return false;
    }

    bool sourcesTagFound = false;
    if (Parser.BeginParseObject())
    {
        do
        {
            const std::wstring key(Parser.GetKey());

            if (_wcsnicmp(key.c_str(), JSON_TAG_SOURCES, _countof(JSON_TAG_SOURCES)) == 0)
            {
                if (Parser.GetNextDataType() != JsonFileParser::DataType::Array)
                {
                    logWriter.TraceError(L"Failed to parse configuration file. 'sources' attribute expected to be an array");
                    Parser.SkipValue();
                    continue;
                }

                sourcesTagFound = true;

                if (!Parser.BeginParseArray())
                {
                    continue;
                }

                do
                {
                    AttributesMap sourceAttributes;

                    //
                    // Read all attributes of a source from the config file,
                    // instantiate it and add it to the end of the vector
                    //
                    if (ReadSourceAttributes(Parser, sourceAttributes)) {
                        if (!AddNewSource(Parser, sourceAttributes, Config.Sources))
                        {
                            logWriter.TraceWarning(L"Failed to parse configuration file. Error reading invalid source.");
                        }
                    }
                    else
                    {
                        logWriter.TraceWarning(L"Failed to parse configuration file. Error retrieving source attributes. Invalid source");
                    }

                    for (auto attributePair : sourceAttributes)
                    {
                        if (attributePair.second != nullptr)
                        {
                            delete attributePair.second;
                        }
                    }
                } while (Parser.ParseNextArrayElement());
            }
            else
            {
                logWriter.TraceWarning(Utility::FormatString(L"Error parsing configuration file. 'Unknow key %ws in the configuration file.", key.c_str()).c_str());
                Parser.SkipValue();
            }
        } while (Parser.ParseNextObjectElement());
    }

    return sourcesTagFound;
}

///
/// Look for all the attributes that a single 'source' object contains
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Config       Returns an AttributesMap, with all allowed tag names and theirs values
///
/// \return True if the attributes contained valid values. Otherwise false
///
bool
ReadSourceAttributes(
    _In_ JsonFileParser& Parser,
    _Out_ AttributesMap& Attributes
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        logWriter.TraceError(L"Failed to parse configuration file. Source item expected to be an object");
        Parser.SkipValue();
        return false;
    }

    bool success = true;

    if (Parser.BeginParseObject())
    {
        do
        {
            //
            // If source reading already fail, just skip attributes^M
            //
            if (!success)
            {
                Parser.SkipValue();
                continue;
            }

            const std::wstring key(Parser.GetKey());

            if (_wcsnicmp(key.c_str(), JSON_TAG_TYPE, _countof(JSON_TAG_TYPE)) == 0)
            {
                const auto& typeString = Parser.ParseStringValue();
                LogSourceType* type = nullptr;

                //
                // Check if the string is the name of a valid LogSourceType
                //
                int sourceTypeArraySize = sizeof(LogSourceTypeNames) / sizeof(LogSourceTypeNames[0]);
                for (int i = 0; i < sourceTypeArraySize; i++)
                {
                    if (_wcsnicmp(typeString.c_str(), LogSourceTypeNames[i], typeString.length()) == 0)
                    {
                        type = new LogSourceType;
                        *type = static_cast<LogSourceType>(i);
                    }
                }

                //
                // If the value isn't a valid type, fail.
                //
                if (type == nullptr)
                {
                    logWriter.TraceError(
                        Utility::FormatString(
                            L"Error parsing configuration file. '%s' isn't a valid source type", typeString.c_str()
                        ).c_str()
                    );

                    success = false;
                }
                else
                {
                    Attributes[key] = type;
                }
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_CHANNELS, _countof(JSON_TAG_CHANNELS)) == 0)
            {
                if (Parser.GetNextDataType() != JsonFileParser::DataType::Array)
                {
                    logWriter.TraceError(L"Error parsing configuration file. 'channels' attribute expected to be an array");
                    Parser.SkipValue();
                    continue;
                }

                if (Parser.BeginParseArray())
                {
                    std::vector<EventLogChannel>* channels = new std::vector<EventLogChannel>();

                    //
                    // Get only the valid channels of this JSON object.
                    //
                    do
                    {
                        channels->emplace_back();
                        if (!ReadLogChannel(Parser, channels->back()))
                        {
                            logWriter.TraceWarning(L"Error parsing configuration file. Discarded invalid channel (it must have a non-empty 'name').");
                            channels->pop_back();
                        }
                    } while (Parser.ParseNextArrayElement());

                    Attributes[key] = channels;
                }
            }
            //
            // These attributes are string type
            // * directory
            // * filter
            //
            else if (_wcsnicmp(key.c_str(), JSON_TAG_DIRECTORY, _countof(JSON_TAG_DIRECTORY)) == 0 ||
                     _wcsnicmp(key.c_str(), JSON_TAG_FILTER, _countof(JSON_TAG_FILTER)) == 0)
            {
                Attributes[key] = new std::wstring(Parser.ParseStringValue());
            }
            //
            // These attributes are boolean type
            // * eventFormatMultiLine
            // * startAtOldestRecord
            // * includeSubdirectories
            // * includeFileNames
            //
            else if (_wcsnicmp(key.c_str(), JSON_TAG_FORMAT_MULTILINE, _countof(JSON_TAG_FORMAT_MULTILINE)) == 0 ||
                     _wcsnicmp(key.c_str(), JSON_TAG_START_AT_OLDEST_RECORD, _countof(JSON_TAG_START_AT_OLDEST_RECORD)) == 0 ||
                     _wcsnicmp(key.c_str(), JSON_TAG_INCLUDE_SUBDIRECTORIES, _countof(JSON_TAG_INCLUDE_SUBDIRECTORIES)) == 0 ||
                     _wcsnicmp(key.c_str(), JSON_TAG_INCLUDE_FILENAMES, _countof(JSON_TAG_INCLUDE_FILENAMES)) == 0)
            {
                Attributes[key] = new bool{ Parser.ParseBooleanValue() };
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDERS, _countof(JSON_TAG_PROVIDERS)) == 0)
            {
                if (Parser.GetNextDataType() != JsonFileParser::DataType::Array)
                {
                    logWriter.TraceError(L"Error parsing configuration file. 'providers' attribute expected to be an array");
                    Parser.SkipValue();
                    continue;
                }

                if (Parser.BeginParseArray())
                {
                    std::vector<ETWProvider>* providers = new std::vector<ETWProvider>();

                    //
                    // Get only the valid providers of this JSON object.
                    //
                    do
                    {
                        providers->emplace_back();
                        if (!ReadETWProvider(Parser, providers->back()))
                        {
                            logWriter.TraceWarning(L"Error parsing configuration file. Discarded invalid provider (it must have a non-empty 'providerName' or 'providerGuid').");
                            providers->pop_back();
                        }
                    } while (Parser.ParseNextArrayElement());

                    Attributes[key] = providers;
                }
            }
            else
            {
                //
                // Discart unwanted attributes
                //
                Parser.SkipValue();
            }
        } while (Parser.ParseNextObjectElement());
    }

    return success;
}

///
/// Reads a single 'channel' object from the parser, and store it in the Result
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Result       Returns an EventLogChannel struct filled with the values specified in the config file.
///
/// \return True if the channel is valid. Otherwise false
///
bool
ReadLogChannel(
    _In_ JsonFileParser& Parser,
    _Out_ EventLogChannel& Result
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        logWriter.TraceError(L"Error parsing configuration file. Channel item expected to be an object");
        Parser.SkipValue();
        return false;
    }

    if (!Parser.BeginParseObject())
    {
        logWriter.TraceError(L"Error parsing configuration file. Error reading channel object");
        return false;
    }

    do
    {
        const auto& key = Parser.GetKey();

        if (_wcsnicmp(key.c_str(), JSON_TAG_CHANNEL_NAME, _countof(JSON_TAG_CHANNEL_NAME)) == 0)
        {
            //
            // Recover the name of the channel
            //
            Result.Name = Parser.ParseStringValue();
        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_CHANNEL_LEVEL, _countof(JSON_TAG_CHANNEL_LEVEL)) == 0)
        {
            //
            // Get the level as a string, and convert it to EventChannelLogLevel.
            //
            std::wstring logLevelStr = Parser.ParseStringValue();
            bool success = Result.SetLevelByString(logLevelStr);

            //
            // Print an error message the string doesn't matches with a valid log level name.
            //
            if (!success)
            {
                logWriter.TraceWarning(
                    Utility::FormatString(
                        L"Error parsing configuration file. '%s' isn't a valid log level. Setting 'Error' level as default", logLevelStr.c_str()
                    ).c_str()
                );
            }
        }
        else
        {
            //
            // Discart unwanted attributes
            //
            Parser.SkipValue();
        }
    } while (Parser.ParseNextObjectElement());


    return Result.IsValid();
}

///
/// Reads a single 'provider' object from the parser, and return it in the Result param
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Result       Returns an ETWProvider struct filled with the values specified in the config file.
///
/// \return True if the channel is valid. Otherwise false
///
bool
ReadETWProvider(
    _In_ JsonFileParser& Parser,
    _Out_ ETWProvider& Result
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        logWriter.TraceError(L"Error parsing configuration file. Provider item expected to be an object");
        Parser.SkipValue();
        return false;
    }

    if (!Parser.BeginParseObject())
    {
        logWriter.TraceError(L"Error parsing configuration file. Error reading provider object");
        return false;
    }

    do
    {
        const auto& key = Parser.GetKey();

        if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDER_NAME, _countof(JSON_TAG_PROVIDER_NAME)) == 0)
        {
            //
            // Recover the name of the provider
            //
            Result.ProviderName = Parser.ParseStringValue();
        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDER_GUID, _countof(JSON_TAG_PROVIDER_GUID)) == 0)
        {
            //
            // Recover the GUID of the provider. If the string isn't a valid
            // GUID, ProviderGuidStr will be empty
            //
            Result.SetProviderGuid(Parser.ParseStringValue());

        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDER_LEVEL, _countof(JSON_TAG_PROVIDER_LEVEL)) == 0)
        {
            //
            // Get the level as a string, and convert it to EventChannelLogLevel.
            //
            std::wstring logLevelStr = Parser.ParseStringValue();
            bool success = Result.StringToLevel(logLevelStr);

            //
            // Print an error message the string doesn't matches with a valid log level name.
            //
            if (!success)
            {
                logWriter.TraceWarning(
                    Utility::FormatString(
                        L"Error parsing configuration file. '%s' isn't a valid log level. Setting 'Error' level as default", logLevelStr.c_str()
                    ).c_str()
                );
            }
        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_KEYWORDS, _countof(JSON_TAG_KEYWORDS)) == 0)
        {
            //
            // Recover the GUID of the provider
            //
            Result.Keywords = wcstoull(Parser.ParseStringValue().c_str(), NULL, 0);

        }
        else
        {
            //
            // Discart unwanted attributes
            //
            Parser.SkipValue();
        }
    } while (Parser.ParseNextObjectElement());


    return Result.IsValid();
}

///
/// Converts the attributes to a valid Source and add it to the vector Sources
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Attributes   An AttributesMap that contains the attributes of the new source objet.
/// \param Sources      A vector, where the new source is going to be inserted after been instantiated.
///
/// \return True if Source was created and added successfully. Otherwise false
///
bool
AddNewSource(
    _In_ JsonFileParser& Parser,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource> >& Sources
    )
{
    //
    // Check the source has a type.
    //
    if (Attributes.find(JSON_TAG_TYPE) == Attributes.end()
        || Attributes[JSON_TAG_TYPE] == nullptr)
    {
        return false;
    }

    switch (*(LogSourceType*)Attributes[JSON_TAG_TYPE])
    {
    case LogSourceType::EventLog:
    {
        std::shared_ptr<SourceEventLog> sourceEventLog = std::make_shared< SourceEventLog>();

        //
        // Fill the new EventLog source object, with its attributes
        //
        if (!SourceEventLog::Unwrap(Attributes, *sourceEventLog))
        {
            logWriter.TraceError(L"Error parsing configuration file. Invalid EventLog source (it must have a non-empty 'channels')");
            return false;
        }

        Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceEventLog)));

        break;
    }

    case LogSourceType::File:
    {
        std::shared_ptr<SourceFile> sourceFile = std::make_shared< SourceFile>();

        //
        // Fill the new File source object, with its attributes
        //
        if (!SourceFile::Unwrap(Attributes, *sourceFile))
        {
            logWriter.TraceError(L"Error parsing configuration file. Invalid File source (it must have a non-empty 'directory')");
            return false;
        }

        Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceFile)));

        break;
    }

    case LogSourceType::ETW:
    {
        std::shared_ptr<SourceETW> sourceETW = std::make_shared< SourceETW>();

        //
        // Fill the new ETW source object, with its attributes
        //
        if (!SourceETW::Unwrap(Attributes, *sourceETW))
        {
            logWriter.TraceError(L"Error parsing configuration file. Invalid ETW source (it must have a non-empty 'providers')");
            return false;
        }

        Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceETW)));

        break;
    }
   
    }
    return true;
}

///
/// Debug function
///
void _PrintSettings(_Out_ LoggerSettings& Config)
{
    std::wprintf(L"LogConfig:\n");
    std::wprintf(L"\tsources:\n");

    for (auto source : Config.Sources)
    {
        switch (source->Type)
        {
        case LogSourceType::EventLog:
        {
            std::wprintf(L"\t\tType: EventLog\n");
            std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(source);

            std::wprintf(L"\t\teventFormatMultiLine: %ls\n", sourceEventLog->EventFormatMultiLine ? L"true" : L"false");
            std::wprintf(L"\t\tstartAtOldestRecord: %ls\n", sourceEventLog->StartAtOldestRecord ? L"true" : L"false");

            std::wprintf(L"\t\tChannels (%d):\n", (int)sourceEventLog->Channels.size());
            for (auto channel : sourceEventLog->Channels)
            {
                std::wprintf(L"\t\t\tName: %ls\n", channel.Name.c_str());
                std::wprintf(L"\t\t\tLevel: %d\n", (int)channel.Level);
                std::wprintf(L"\n");
            }
            std::wprintf(L"\n");

            break;
        }
        case LogSourceType::File:
        {
            std::wprintf(L"\t\tType: File\n");
            std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(source);

            std::wprintf(L"\t\tDirectory: %ls\n", sourceFile->Directory.c_str());
            std::wprintf(L"\t\tFilter: %ls\n", sourceFile->Filter.c_str());
            std::wprintf(L"\t\tIncludeSubdirectories: %ls\n", sourceFile->IncludeSubdirectories ? L"true" : L"false");
            std::wprintf(L"\t\includeFileNames: %ls\n", sourceFile->IncludeFileNames ? L"true" : L"false");
            std::wprintf(L"\n");

            break;
        }
        case LogSourceType::ETW:
        {
            std::wprintf(L"\t\tType: ETW\n");

            std::shared_ptr<SourceETW> sourceETW = std::reinterpret_pointer_cast<SourceETW>(source);

            std::wprintf(L"\t\teventFormatMultiLine: %ls\n", sourceETW->EventFormatMultiLine ? L"true" : L"false");

            std::wprintf(L"\t\tProviders (%d):\n", (int)sourceETW->Providers.size());
            for (auto provider : sourceETW->Providers)
            {
                std::wprintf(L"\t\t\tProviderName: %ls\n", provider.ProviderName.c_str());
                std::wprintf(L"\t\t\tProviderGuid: %ls\n", provider.ProviderGuidStr.c_str());
                std::wprintf(L"\t\t\tLevel: %d\n", (int)provider.Level);
                std::wprintf(L"\t\t\tKeywords: %llx\n", provider.Keywords);
                std::wprintf(L"\n");
            }
            std::wprintf(L"\n");

            break;
        }
        } // Switch
    }
}
