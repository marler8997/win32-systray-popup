@where cl >nul 2>&1
@if %errorlevel% neq 0 (
    echo The MSVC compiler "cl" is not found in PATH, are you running inside a Visual Studio Command Prompt?
    exit /b 1
)
@where mt >nul 2>&1
@if %errorlevel% neq 0 (
    echo The MSVC compiler "mt" is not found in PATH, are you running inside a Visual Studio Command Prompt?
    exit /b 1
)

del SystrayPopup.ilk

cl SystrayPopup.c /Z7
@if %errorlevel% neq 0 exit /b %errorlevel%

@REM We include a manifest file to indicate that our application is DPI-Aware.
@REM This is important to include because when using the NOTIFYICON_VERSION_4
@REM option for Shell_NotifyIcon, the coordinates given will not be correct if
@REM the application is not DpiAware.  For legacy applications that cannot
@REM support Dpi awareness, they should use the older NOTIFYICON_VERSION Api
@REM along with GetCursorPos.
@REM     see https://stackoverflow.com/questions/41649303/difference-between-notifyicon-version-and-notifyicon-version-4-used-in-notifyico

mt -manifest SystrayPopup.manifest -outputresource:SystrayPopup.exe;1
@if %errorlevel% neq 0 exit /b %errorlevel%

SystrayPopup.exe
