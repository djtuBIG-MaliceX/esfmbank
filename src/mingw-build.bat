@echo off
:: mingw shit
set LIBS=-static -lm -mwindows -lcfgmgr32 -lwinmm -lsetupapi -lshlwapi -lcomctl32 -U_WIN64 -fpermissive

del *.o
del *.exe

:: intermediate compile
g++ -c esdev.c %LIBS% -o esdev.o
g++ -c esfmbank.c %LIBS% -o esfmbank.o
g++ -c essplaymid\esfm.c %LIBS% -o esfm.o
g++ -c essplaymid\natv.c %LIBS% -o natv.o
g++ -c essplaymid\iodriver\drv_inpout32.c %LIBS% -o drv_inpout32.o
g++ -c essplaymid\iodriver\InpOut32Helper.cpp %LIBS% -o InpOut32Helper.o


:: gui shit
windres.exe res.rc -o resource.o

:: link
g++ *.o -static -lm -mwindows -lcfgmgr32 -lwinmm -lsetupapi -lshlwapi -lcomctl32 -U_WIN64 -fpermissive -o test.exe
