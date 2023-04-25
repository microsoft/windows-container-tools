@cls
@echo.*************************************************************
@echo.This script simply calls msbuild to build LogMonitor.exe
@echo.*************************************************************
@echo.
msbuild LogMonitor\LogMonitor.sln /p:platform=x64 /p:configuration=Release %*
@if '%ERRORLEVEL%' == '0' (
    echo.
    echo.
    echo.*************************************************************
    echo.               The build was successful!
    echo.The output should be in LogMonitor\x64\Release\LogMonitor.exe
    echo.This is the only file needed to deploy the program.
    echo.*************************************************************
)