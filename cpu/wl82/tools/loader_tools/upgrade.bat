@echo off

color f2
title OTA/SD卡升级  
                        
@echo 请仔细阅读note.txt信息，可以避免你升级时出现的一些问题。

start "" "note.txt"
set /p var=是否已经认真阅读note.txt，确定生成升级文件（y/n），请输入:

if /i %var% == y (
echo "正在生成升级文件..."

if /i %1 == OTA (
echo "当前升级模式：OTA"
copy db_update_files_data.bin upgrade\update-ota.ufw
echo.
echo 升级文件已经在当前upgrade目录下生成，升级文件名称：update-ota.ufw，用在OTA网络升级即可（2秒钟后自动关闭窗口）
)else (

if /i %1 == SD (
echo "当前升级模式：SD"
copy jl_isd.ufw upgrade\update.ufw
echo.
echo 升级文件已经在当前upgrade目录下生成，升级文件名称：update.ufw，将update.ufw拷贝到SD卡/U盘的根目录，插卡上电即可升级（2秒钟后自动关闭窗口）
) else (
echo "当前升级模式：ERROR"
)

)

) else (
echo "生成升级文件失败"
)

choice /t 2 /d y /n >nul 	

exit