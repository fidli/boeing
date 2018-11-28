@echo off
rem call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
cls
set FILES="%cd%\windows64_bug01.cpp"
set LIBS="Kernel32.lib" "Advapi32.lib" "Shell32.lib" "User32.lib" "Ws2_32.lib" "Winusb.lib" "Setupapi.lib"

set BASELIB="%cd%\..\..\baselib"

pushd build

rem CANNOT USE SOME C++ FEATURES, std lib is ripped off (https://hero.handmade.network/forums/code-discussion/t/94)
call cl.exe /nologo /W2 /WX /EHa- /GS- /GR- /Od /Zi /FS /DSERVER /I %BASELIB% /I %LIBDIR% /Fdbeacons64.pdb /Febeacons64.exe %FILES%  /link /INCREMENTAL:NO /NODEFAULTLIB /SUBSYSTEM:CONSOLE %LIBS%
                                                         

POPD

                                                         