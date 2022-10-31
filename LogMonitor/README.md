# Microsoft Log Monitor

## Introduction

**Log Monitor** is a log tool for Windows Containers. It monitors configured log sources and pipes a formatted output to STDOUT.

![log_minitor_arch](https://user-images.githubusercontent.com/261265/199001472-8338a769-8db0-44c7-89bf-e0ab771d57e5.png)

Unlike Linux applications that log to STDOUT, Windows applications log to Windows log locations such as ETW, Event Log, and custom log files. Since many container ecosystem logging solutions are built to pull from the STDOUT pipeline as standard with Linux, Windows containers app logs historically have not been accessible via these solutions. The Log Monitor bridges this gap between Windows log locations and STDOUT, as depicted in the diagram below. The scope of the Log Monitor tool is to bridge Windows application logs to the STDOUT pipeline.

Supported log sources include:

- [Event Logs]((./docs/README.md#event-log-monitoring)
- [ETW Providers](./docs/README.md#etw-monitoring)
- [Custom App logs](./docs/README.md#log-file-monitoring)

Supported output locations include:

- `STDOUT`

Log Monitor is configured via the Log Monitor Config json file. The default location for the config file is: `C:/LogMonitor/LogMonitorConfig.json` or location passed to the `LogMonitor.exe` via `/CONFIG` switch.

The log tool is supported for Windows, Server Core, and Nano images.

## Build

## Releases

Release versions of the binaries can be found on the [Log Monitor Releases page](https://github.com/microsoft/windows-container-tools/releases/latest).

## Usage

`LogMonitor.exe` and `LogMonitorConfig.json` should both be included in the same `LogMonitor` directory. 

The Log Monitor tool can either be used in a SHELL usage pattern:

```dockerfile
SHELL ["C:\\LogMonitor\\LogMonitor.exe", "cmd", "/S", "/C"]
CMD "C:\\windows\\system32\\ping.exe -n 20 localhost"
```

Or an `ENTRYPOINT` usage pattern:

```dockerfile
ENTRYPOINT "C:\\LogMonitor\\LogMonitor.exe c:\\windows\\system32\\ping.exe -n 20 localhost"
```

Both example usages wrap the ping.exe application. Other applications (such as [IIS.ServiceMonitor]( https://github.com/microsoft/IIS.ServiceMonitor)) can be nested with Log Monitor in a similar fashion:

```dockerfile
COPY "LogMonitor.exe LogMonitorConfig.json C:\\LogMonitor"
WORKDIR /LogMonitor
SHELL ["C:\\LogMonitor\\LogMonitor.exe", "powershell.exe"]
 
# Start IIS Remote Management and monitor IIS
ENTRYPOINT      Start-Service WMSVC; `
                    C:\ServiceMonitor.exe w3svc;
```


Log Monitor starts the wrapped application as a child process and monitors the STDOUT output of the application.

Note that in the `SHELL` usage pattern the `CMD`/`ENTRYPOINT` instruction should be specified in the `SHELL` form and not exec form. When exec form of the `CMD`/`ENTRYPOINT` instruction is used, SHELL is not launched, and the Log Monitor tool will not be launched inside the container.

The repo includes several sample config files for key Windows Container scenarios. For more detail on how to author the config file, see the [detailed documentation here](./docs/README.md).

👉 **[See more Documentation](./docs/README.md)**
