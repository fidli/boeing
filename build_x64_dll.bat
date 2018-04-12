@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
cls
set FILES="%cd%\sources\servercode.cpp"
set LIBS="Kernel32.lib" "Advapi32.lib" "Shell32.lib" "User32.lib" "Gdi32.lib" "Ws2_32.lib"
set BASELIB="%cd%\baselib"
pushd build

set preunique=%time: =0%
set unique=%preunique:~0,2%%preunique:~3,2%%preunique:~6,2%%preunique:~9,2%


rem CANNOT USE SOME C++ FEATURES, std lib is ripped off (https://hero.handmade.network/forums/code-discussion/t/94)
call cl.exe /nologo /W2 /WX /EHa- /GS- /GR- /Od /Zi /FS /DSERVER /I %BASELIB%  /I %LIBDIR% /Fd"gamecode_%unique%.pdb" /Feservercode.dll %FILES%  /link /INCREMENTAL:NO /NODEFAULTLIB /DLL %LIBS% /PDB:"gamecode_%unique%.pdb"


POPD