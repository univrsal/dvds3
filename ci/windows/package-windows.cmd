call "%~dp0..\ci_includes.generated.cmd"

mkdir package
cd package

git rev-parse --short HEAD > package-version.txt
set /p PackageVersion=<package-version.txt
del package-version.txt

REM Package ZIP archive
7z a "%PluginName%.v%PackageVersion%-win.zip" "..\release\*"

REM Build installer
iscc ..\package\installer-Windows.generated.iss /O. /F"%PluginName%.v%PackageVersion%-win"
