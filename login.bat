@echo off
setlocal

set "CLIENT=%~dp0build-qt\client\QtUserClient\QtUserClient.exe"

if not exist "%CLIENT%" (
    echo Qt client not found. Build the project first.
    pause
    exit /b 1
)

rem 默认只启动客户端，由用户自己输入账号密码登录。
rem 如需自动登录，显式传参：login.bat admin 123456
set "USERNAME=%~1"
set "PASSWORD=%~2"
if "%USERNAME%"=="" goto showlogin
if "%PASSWORD%"=="" goto showlogin

start "Reactor User Workbench" "%CLIENT%" --auto-login --username "%USERNAME%" --password "%PASSWORD%"
goto end

:showlogin
start "Reactor User Workbench" "%CLIENT%"

:end
endlocal
