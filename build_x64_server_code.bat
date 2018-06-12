@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
cls
set FILES="%cd%\sources\servercode.cpp"
set LIBS="Kernel32.lib" "Advapi32.lib" "Shell32.lib" "User32.lib" "Gdi32.lib" "Ws2_32.lib"
set BASELIB="%cd%\baselib"
set SOURCES="%cd%\sources"
pushd build

set preunique=%time: =0%
set unique=%preunique:~0,2%%preunique:~3,2%%preunique:~6,2%%preunique:~9,2%


rem CANNOT USE SOME C++ FEATURES, std lib is ripped off (https://hero.handmade.network/forums/code-discussion/t/94)
call cl.exe /nologo /LD /DCRT_PRESENT /DPRECISE_MATH /EHsc /FI "C:\Program Files (x86)\Windows Kits\8.1\Include\um\winsock2.h" /FI "C:\Program Files (x86)\Windows Kits\8.1\Include\um\ws2tcpip.h" /FI "C:\Program Files (x86)\Windows Kits\8.1\Include\um\Windows.h" /FI "%BASELIB%/windows_types.h" /FI "%SOURCES%/servercode_memory.h" /FI "%BASELIB%/windows_net.cpp" /W2 /WX /EHa- /GS- /GR- /Od /Zi /FS /DSERVER /I %BASELIB%  /I %LIBDIR% /Fd"servercode_%unique%.pdb" /Feservercode.dll %FILES%  /link /INCREMENTAL:NO %LIBS% /PDB:"servercode_%unique%.pdb"


popd