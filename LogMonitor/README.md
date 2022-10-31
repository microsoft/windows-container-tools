# Microsoft Log Monitor

## Introduction

**Log Monitor** is a log tool for Windows Containers. It monitors configured log sources and pipes a formatted output to a configured output location. Supported log sources include:

- Event Logs
- ETW Providers
- Custom App logs

Supported output locations include:

- `STDOUT`

Log Monitor is configured via the Log Monitor Config json file.

## Build

## Releases

Release versions of the binaries can be found on the Log Monitor Releases page.

## Usage

LogMonitor.exe and LogMonitorConfig.json should both be included in the same LogMonitor directory. 

The Log Monitor tool can either be used in a SHELL usage pattern:

```docker
SHELL ["C:\\LogMonitor\\LogMonitor.exe", "cmd", "/S", "/C"]
CMD c:\windows\system32\ping.exe -n 20 localhost
```

Or an `ENTRYPOINT` usage pattern:

```docker
ENTRYPOINT C:\LogMonitor\LogMonitor.exe c:\windows\system32\ping.exe -n 20 localhost
```

Both example usages wrap the ping.exe application. Other applications (such as [IIS.ServiceMonitor]( https://github.com/microsoft/IIS.ServiceMonitor)) can be nested with Log Monitor in a similar fashion:

```docker
COPY LogMonitor.exe LogMonitorConfig.json C:\LogMonitor\
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