@echo off

@echo *********************************************************************
@echo 			    AC791N SDK 网络升级文件生成
@echo *********************************************************************
@echo %date%

::先执行download.bat生成文件
call .\download.bat

if exist upgrade\*.ufw del upgrade\*.ufw

if not exist upgrade md upgrade

set mode=OTA
start .\upgrade.bat %mode%

choice /t 2 /d y /n >nul 

::pause
exit