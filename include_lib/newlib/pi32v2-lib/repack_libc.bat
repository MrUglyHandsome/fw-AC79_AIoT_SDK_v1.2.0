@echo off

set AR=C:\JL\pi32\bin\pi32v2-lto-ar.exe
set DFILES=(lib_a-strdup.o lib_a-atof.o)

if not exist Temp md Temp

cd Temp

%AR% x ..\libc.a

for %%a in %DFILES% do (
	if exist %%a del %%a
)

setlocal enabledelayedexpansion
set Files=
for %%i in (*.o) do set Files=!Files! %%i
echo %Files%

%AR% rcu libc.a %Files%

copy libc.a ../libc.a

cd ..\
if exist Temp rd /s /q Temp

pause