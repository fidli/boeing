@echo off
rem call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
cls
set FILES="%cd%\sources\windows64_beacons.cpp"
set DOMAIN_FILES="%cd%\sources\beacons_domain.cpp"
set LIBS="Kernel32.lib" "Advapi32.lib" "Shell32.lib" "User32.lib" "Ws2_32.lib" "Winusb.lib" "Setupapi.lib"


set BASELIB="%cd%\baselib"
set SOURCEFOLD="%cd%\sources"


rem CANNOT USE SOME C++ FEATURES, std lib is ripped off (https://hero.handmade.network/forums/code-discussion/t/94)
call cl.exe /nologo /W2 /WX /EHa- /GS- /GR- /Od /Zi /FS /DSERVER /I %BASELIB% /I %LIBDIR% /Fdbeacons64.pdb /Febeacons64.exe %FILES%  /link /INCREMENTAL:NO /NODEFAULTLIB /SUBSYSTEM:CONSOLE %LIBS%
                                                         
set preunique=%time: =0%
set unique=%preunique:~0,2%%preunique:~3,2%%preunique:~6,2%%preunique:~9,2%
                                                         
call cl.exe /nologo /LD /W2 /WX /Od /GS- /Zi /FS /DSERVER /Fd"beacons_domain_%unique%.pdb" /Febeacons_domain.dll /I %BASELIB% /FI "C:\Program Files (x86)\Windows Kits\8.1\Include\um\winsock2.h" /FI "C:\Program Files (x86)\Windows Kits\8.1\Include\um\ws2tcpip.h" /FI "C:\Program Files (x86)\Windows Kits\8.1\Include\um\Windows.h" /FI "%BASELIB%\windows_types.h" /FI "%BASELIB%\common.h" /FI "%BASELIB%\windows_net.cpp" /FI "%BASELIB%\windows_serial.cpp" /FI "%SOURCEFOLD%\beacons_domain_mem.h"  /FI "%BASELIB%\windows_time.cpp" /FI "%BASELIB%\util_mem.h" /FI "%BASELIB%\windows_io.cpp"  %DOMAIN_FILES%  /link /INCREMENTAL:NO /NODEFAULTLIB %LIBS% /PDB:"beacons_domain_%unique%.pdb"

cp -f beacons64.exe \\FIDLI-LAPTOP\hole\beacons64.exe
cp -f beacons_domain.dll \\FIDLI-LAPTOP\hole\beacons_domain.dll
cp -f data\beacons.config \\FIDLI-LAPTOP\hole\data\beacons.config

POPD
                                                         
                                                         