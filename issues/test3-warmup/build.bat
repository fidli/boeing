@echo off
rem call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
cls
set FILES="%cd%\windows64_test_warmup.cpp"
set LIBS="Kernel32.lib" "Advapi32.lib" "Shell32.lib" "User32.lib"

set BASELIB="%cd%\..\..\baselib"

pushd build

call cl.exe /nologo /W2 /WX /EHar /GS- /GR- /Od /Zi /FS /DSERVER /DPRECISE_MATH /I %BASELIB% /I %LIBDIR% /Fdtest1.pdb /Fetest1.exe %FILES%  /link /INCREMENTAL:NO /SUBSYSTEM:CONSOLE %LIBS%
                                                         

POPD

                                                         