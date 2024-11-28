@echo off

JLpack.exe -f file1.txt -v 3.4.5 -f file2.txt -v 2.1.2  -o out.txt

REM Package two upgrade files
::JLpack.exe -f exAppUpdate.ufw -v 3.4.5 -f appUpdate.ufw -v 2.1.2  -o otaUpdate.ufw 

REM Package only one upgrade file
::JLpack.exe -f appUpdate.ufw -v 3.4.6 -o otaUpdate.ufw 
pause