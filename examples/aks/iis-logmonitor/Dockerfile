# escape=`
FROM mcr.microsoft.com/windows/servercore:ltsc2022
WORKDIR /LogMonitor
COPY LogMonitorConfig.json .

# NOTE: replace the version (v2.1.3) in the URL below with the latest
# LogMonitor release or with the specific version/build you want to use.
RUN powershell.exe -command `
    wget `
        -uri "https://github.com/microsoft/windows-container-tools/releases/download/v2.1.3/LogMonitor.exe" `
        -outfile "LogMonitor.exe"
        
RUN powershell -Command `
    Add-WindowsFeature Web-Server; `
    Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/microsoft/IIS.ServiceMonitor/releases/download/v2.0.1.10/ServiceMonitor.exe" -OutFile "C:\ServiceMonitor.exe"

EXPOSE 80

ENTRYPOINT ["C:\\LogMonitor\\LogMonitor.exe", "C:\\ServiceMonitor.exe", "w3svc"]
