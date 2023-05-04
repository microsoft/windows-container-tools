# Log Monitor Documentation

**Contents:**
- [Logs Customization](#logs-customization)
- [Sample Config File](#sample-config-file)
- [ETW Monitoring](#etw-monitoring)
- [Event Log Monitoring](#event-log-monitoring)
- [Log File Monitoring](#log-file-monitoring)
- [Process Monitoring](#process-monitoring)

## Logs Customization

### Description
By default, log monitor's logs will be displayed in JSON format. However, a user has the flexibility to configure the logs to be displayed in either `XML` format or their own `custom` defined format.

To "specify the log format to be used, a user will need to add a new field 'logFormat' in the config file and specify it's value as either `XML`, `JSON` or `Custom` <em>(the field value is case-insensitive)</em>
<br> For the `JSON` and `XML` log formats, no additional configurations are needed. However, for the `Custom` log format, a user needs to specify the custom log format at the source type level.

### Custom Log Format Pattern Layout
To ensure the different fields values you want to display appear in the customized log outputs, ensure to enclose the field names with the percentage sign (%) and the field names specified match the correct field names for the specific log sources.

`For example: %Message%, [%TimeStamp%]`

Different logs source types have different field names:<br> 
<strong>Event Logs:</strong>
  - `Source`: The source of the log. i.e ‘EventLog’ to differentiate the sources of the different logs being streamed in the console.
  - `TimeStamp`: The time the event was logged
  - `EventID`: The event Id
  - `Severity`: A label that indicates the severity or urgency of a log entry
  - `Message`: The event message

<strong>ETW Logs:</strong>
  - `Source`: The Log source (Event Log)
  - `TimeStamp`: The time the event was logged
  - `Severity`: A label that indicates the severity or urgency of a log entry
  - `ProviderId`: The ETW Event Provider ID
  - `ProviderName`: The ETW Event Provider Name
  - `DecodingSource`
  - `ExecutionProcessId`
  - `ExecutionThreadId`
  - `Keyword`
  - `EventId`
  - `EventData`

<strong>File Logs:</strong>
  - `Source`: The Log source (File)
  - `TimeStamp`: The time when the change was introduced in the monitored file.
  - `FileName`: The name of the file that the log entry is read from.
  - `Message`: The line/change added in the monitored file.

### Sample Custom Log Configuration

```json
{
  "LogConfig": {
    "logFormat": "custom",
    "sources": [
      {
        "type": "ETW",
        "eventFormatMultiLine": false,
        "providers": [
          {
            "providerName": "Microsoft-Windows-WLAN-Drive",
            "providerGuid": "DAA6A96B-F3E7-4D4D-A0D6-31A350E6A445",
            "level": "Information"
          }
        ],
        "customLogFormat": "{'TimeStamp':'%TimeStamp%', 'source':'%Source%', 'Severity':'%Severity%', 'ProviderId':'%ProviderId%', 'ProviderName':'%ProviderName%', 'EventId':'%EventId%', 'Message':'%Message%'}"
      },
      {
        "type": "File",
        "directory": "c:\\inetpub\\logs",
        "filter": "*.log",
        "includeSubdirectories": true,
        "customLogFormat": "{'message':%Message%,'source':%Source%,'fileName':%FileName%}"
      }
    ]
  }
}
```

For advanced usage of the custom log feature, a user can choose to define their own custom JSON log format. 
In such a case, The `logFormat` field configured value should be `custom`. In addition to that, to enable sanitization of the JSON output and validation of the outputs displayed by the tool, the user can add a suffix: `'|json'` after the desired custom log format.
For example:
```json
{
  "LogConfig": {
	"logFormat": "custom",
        "sources": [
	      {
                "type": "ETW",
                "eventFormatMultiLine": false,
                "providers": [
                  {
                     "providerName": "Microsoft-Windows-WLAN-Drive",
                    "providerGuid": "DAA6A96B-F3E7-4D4D-A0D6-31A350E6A445",
                    "level": "Information"
                   }
                 ],
                "customLogFormat": "{'TimeStamp':'%TimeStamp%', 'source':'%Source%', 'Severity':'%Severity%', 'ProviderId':'%ProviderId%', 'ProviderName':'%ProviderName%', 'EventId':'%EventId%', 'Message':'%Message%'}|json"
	      }
        ]
  }
}
```

## Sample Config File

A sample Log Monitor Config file would be structured as follows: 

```json
{
  "LogConfig": {
    "sources": [
      {
        "type": "EventLog",
        "startAtOldestRecord": true,
        "eventFormatMultiLine": false,
        "channels": [
          {
            "name": "system",
            "level": "Information"
          },
          {
            "name": "application",
            "level": "Error"
          }
        ]
      },
      {
        "type": "File",
        "directory": "c:\\inetpub\\logs",
        "filter": "*.log",
        "includeSubdirectories": true,
        "includeFileNames": false
      },
      {
        "type": "ETW",
        "eventFormatMultiLine": false,
        "providers": [
          {
            "providerName": "IIS: WWW Server",
            "providerGuid": "3A2A4E84-4C21-4981-AE10-3FDA0D9B0F83",
            "level": "Information"
          },
          {
            "providerName": "Microsoft-Windows-IIS-Logging",
            "providerGuid": "7E8AD27F-B271-4EA2-A783-A47BDE29143B",
            "level": "Information"
          }
        ]
      }
    ]
  }
}

```
Please see below for how to customize your Config file for Log Monitor to pull from.

## ETW Monitoring

### Description

ETW (Event Tracing for Windows) is a system for routing events. ETW is primarily intended for diagnostic purposes and is optimized to minimize impact on the overall system performance. Please note that ETW is different from the Windows Event Log.

To monitor ETW logs using Log Monitor you require a valid _provider GUID_ (Globally Unique Identifier).

The _provider GUID_ that you pass in the JSON config file must be registered with ETW. Log Monitor currently allows you to monitor events generated by MOF (classic) providers, manifest-based providers and Trace Logging providers. _**Note**: we do not monitor log events from WPP providers._

You can see all the providers on your system using the `logman query providers` command.

To confirm if you have a valid provider name or valid provider GUID you can type the command below in command prompt or PowerShell.

```cmd
logman query providers | findstr "<GUID or Provider Name>" 
```

### Configuration

- `type` (required): This indicates the type of log you want to monitor for. It should be `ETW`. 
- `eventFormatMultiLine` (optional): This is Boolean to indicate whether you want the logs displayed with or without new lines. It is initially set to True and you can set it to False depending on how you want to view the logs on the console.
- `providers` (required): Providers are components that generate events. This field is a list that shows the event providers you are monitoring for.
    - `providerName` (optional): This represents the name of the provider. It is what shows up when you use logman.
    - `providerGuid` (required): This is a globally unique identifier that uniquely identifies the provider you specified in the ProviderName field.
    - `level` (optional): This string field specifies the verboseness of the events collected. These include `Critical`, `Error`, `Warning`, `Information` and `Verbose`. If the level is not specified, level will be set to `Error`.
    - `keywords` (optional): This string field is a bitmask that specifies what events to collect. Only events with keywords matching the bitmask are collected This is an optional parameter. Default is 0 and all the events will be collected.

### Examples

Using only the provider's GUID in your configuration:

```json
{
  "LogConfig": {
    "sources": [
      {
        "type": "ETW",
        "eventFormatMultiLine": false,
        "providers": [
          {
            "providerGuid": "DAA6A96B-F3E7-4D4D-A0D6-31A350E6A445",
            "level": "Information"
          }
        ]
      }
    ]
  }
}
```

Using both the provider's name and provider GUID:

```json
{
  "LogConfig": {
    "sources": [
      {
        "type": "ETW",
        "eventFormatMultiLine": false,
        "providers": [
          {
            "providerName": "Microsoft-Windows-WLAN-Drive",
            "providerGuid": "DAA6A96B-F3E7-4D4D-A0D6-31A350E6A445",
            "level": "Information"
          }
        ]
      }
    ]
  }
}
```

### References

- https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/event-tracing-for-windows--etw-
- https://learn.microsoft.com/en-us/windows/win32/wes/writing-an-instrumentation-manifest
- https://learn.microsoft.com/en-us/windows/win32/etw/about-event-tracing#providers

## Event Log Monitoring

### Description

Event log is a record of events related to the system, security, and application stored on a Windows operating system. The event log provides a standard, centralized way for apps and the operating system to record important software and hardware events, helping in tracking of potential threats and problems that could be potentially degrading performance.

### Configuration

- `startAtOldestRecord` (Required): This Boolean field indicates whether the Log Monitor tool should output event logs from the start of the container boot or from the start of the Log Monitor tool itself. If set `true`, the tool should output the event logs from the start of container boot, and if set false, the tool only outputs event logs from the start of log monitor.
- `eventFormatMultiLine` (Optional): This is a Boolean field that is used to indicate whether the Log Monitor should format the logs to `STDOUT` as multi-line or single line. If the field is not set in the config file, by default the value is `true`. If the field is set `true`, the tool does not format the event messages to a single line (and thus event messages can span multiple lines). If set to false, the tool formats the event log messages to a single line and removes new line characters.
- `channels` (Required): A channel is a named stream of events. It serves as a logical pathway for transporting events from the event publisher to a log file and possibly a subscriber. It is a sink that collects events. Each defined channel has the following properties:
    - `name` (Required): The name of the event channel
    - `level` (optional): This string field specifies the verboseness of the events collected. These include `Critical`, `Error`, `Warning`, `Information` and `Verbose`. If the level is not specified, level will be set to `Error`.

### Examples

Example 1 (Application channel, verboseness: Error):

 ```json
{
  "LogConfig": {
    "sources": [
      {
        "type": "EventLog",
        "startAtOldestRecord": true,
        "eventFormatMultiLine": false,
        "channels": [
          {
            "name": "application",
            "level": "Error"
          }
        ]
      }
    ]
  }
}
 ```

 Example 2 (System channel, verboseness: Information):

 ```json
{
  "LogConfig": {
    "sources": [
      {
        "type": "EventLog",
        "startAtOldestRecord": true,
        "eventFormatMultiLine": true,
        "channels": [
          {
            "name": "system",
            "level": " Information"
          }
        ]
      }
    ]
  }
}
 ```

 ### References

 - https://learn.microsoft.com/en-us/windows/win32/eventlog/event-logging
 - https://learn.microsoft.com/en-us/windows/win32/wes/defining-channels

## Log File Monitoring

### Description

This will monitor any changes in log files matching a specified filter, given the log directory location in the configuration. For instance, given a directory like `C:\temp\logs` and `*.log` filter, this will monitor any changes in `.log` files within the `C:\temp\logs` directory.

### Configuration

- `type` (required): `"File"`
- `directory` (required): set to the directory containing the files to be monitored.
- `filter` (optional): uses [MS-DOS wildcard match type](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/indexsrv/ms-dos-and-windows-wildcard-characters) i.e.. `*, ?`. Can be set to empty, which will be default to `"*"`.
- `includeSubdirectories` (optional) : `"true|false"`, specify if sub-directories also need to be monitored. Defaults to `false`.
- `includeFileNames` (optional): `"true|false"`, specifies whether to include file names in the logline, eg. `sample.log: xxxxx`. Defaults to `false`.


### Examples

```json
{
  "LogConfig": {
    "sources": [
      {
        "type": "File",
        "directory": "c:\\inetpub\\logs",
        "filter": "*.log",
        "includeSubdirectories": true,
        "includeFileNames": false
      }
    ]
  }
}
```

## Process Monitoring

### Description

Currently, this one does not need any configuration, it basically streams the output of tthe process/command that is provided as the argument to `LogMonitor.exe`. For the given _entryproint_ command, a child process is created and links its STDIN and STDOUT to the `LogMonitor` process (through a _pipe_).

### Examples

```dockerfile
SHELL ["C:\\LogMonitor\\LogMonitor.exe", "cmd", "/S", "/C"] 
CMD "c:\\windows\\system32\\ping.exe -n 20 localhost" 
```

The Process Monitor will stream the output for `c:\windows\system32\ping.exe -n 20 localhost`
