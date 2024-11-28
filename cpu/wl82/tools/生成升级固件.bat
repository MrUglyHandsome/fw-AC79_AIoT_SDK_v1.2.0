@echo off

call .\download.bat
cls

if exist upgrade\*.ufw del upgrade\*.ufw
if not exist upgrade md upgrade

start "" "note.txt"

@echo *********************************************************************
@echo 			        AC791N SDK 升级文件生成
@echo %date%
@echo *********************************************************************

echo [ 升级方式选择 ]
echo 1: SD卡升级、U盘升级方式。[支持单双备份]
echo 2: HTTP OTA、FTP升级、虚拟U盘升级方式。[只支持双备份]
echo q: 取消
echo .

set /p var=升级前请仔细阅读note.txt, 请选择升级方式：    

if %var% EQU 1 (
	copy jl_isd.ufw upgrade\update.ufw
	if exist upgrade\update.ufw (
		echo -----------------------------------------------------------------
		echo .
		echo .
		echo [OK]升级文件已经在当前upgrade目录下生成，升级文件名称：update.ufw
		echo .
		echo -----------------------------------------------------------------
	) else (
		echo -----------------------------------------------------------------
		echo .
		echo .
		echo [ERR]升级文件生成失败，请仔细阅读note.txt
		echo .
		echo -----------------------------------------------------------------
	)
) else if %var% EQU 2 (
	copy db_update_files_data.bin upgrade\update-ota.ufw
	if exist upgrade\update-ota.ufw (
		echo -----------------------------------------------------------------
		echo .
		echo .
		echo [OK]升级文件已经在当前upgrade目录下生成，升级文件名称：update-ota.ufw
		echo .
		echo -----------------------------------------------------------------
	) else (
		echo -----------------------------------------------------------------
		echo .
		echo .
		echo [ERR]升级文件生成失败，请仔细阅读note.txt
		echo .
		echo -----------------------------------------------------------------
	)
) else if %var% EQU q (
    exit
)

pause