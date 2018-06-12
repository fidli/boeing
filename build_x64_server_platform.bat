@echo off
rem call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
cls
set FILES="%cd%\sources\windows64_platform.cpp"
set LIBS="Kernel32.lib" "Advapi32.lib" "Shell32.lib" "User32.lib" "Gdi32.lib" "Ws2_32.lib"
set BASELIB="%cd%\baselib"
pushd build

rem CANNOT USE SOME C++ FEATURES, std lib is ripped off (https://hero.handmade.network/forums/code-discussion/t/94)
call cl.exe /nologo /W2 /WX /EHa- /GS- /GR- /Od /Zi /FS /DSERVER /I %BASELIB% /I %LIBDIR% /Fdserver64.pdb /Feserver64.exe %FILES%  /link /INCREMENTAL:NO /NODEFAULTLIB /SUBSYSTEM:CONSOLE %LIBS%


POPD