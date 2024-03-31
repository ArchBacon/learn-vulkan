@echo off
call ThirdParty\Premake\premake5.exe --file=Vulkan.Build.lua vs2022
IF %ERRORLEVEL% NEQ 0 PAUSE &:: If the output code is not 0, there is probably an error
