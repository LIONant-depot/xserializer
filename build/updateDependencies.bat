@echo OFF
setlocal enabledelayedexpansion
cd %cd%
set XSERIALIZER_PATH="%cd%"


COLOR 8E
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore Cyan Welcome I am your XSERIALIZER dependency updater bot, let me get to work...
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
echo.

:DOWNLOAD_DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XSERIALIZER - DOWNLOADING DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------

echo.
rmdir "../dependencies/xcompression" /S /Q
git clone https://github.com/LIONant-depot/xcompression.git "../dependencies/xcompression"
if %ERRORLEVEL% GEQ 1 goto :ERROR

rmdir "../dependencies/xerr" /S /Q
git clone https://github.com/LIONant-depot/xerr.git "../dependencies/xerr"
if %ERRORLEVEL% GEQ 1 goto :ERROR

rmdir "../dependencies/xfile" /S /Q
git clone https://github.com/LIONant-depot/xfile.git "../dependencies/xfile"
if %ERRORLEVEL% GEQ 1 goto :ERROR


:COMPILATION
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XSERIALIZER - COMPILING DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------

cd ../dependencies/xcompression/build
updateDependencies.bat nopause


:DONE
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XSERIALIZER - SUCCESSFULLY DONE!!
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
goto :PAUSE

:ERROR
powershell write-host -fore Red ------------------------------------------------------------------------------------------------------
powershell write-host -fore Red XSERIALIZER - FAILED!!
powershell write-host -fore Red ------------------------------------------------------------------------------------------------------

:PAUSE
rem if no one give us any parameters then we will pause it at the end, else we are assuming that another batch file called us
if %1.==. pause