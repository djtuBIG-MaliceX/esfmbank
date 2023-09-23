#!/usr/bin/env bash
# mingw shit
#CC="clang++"
CC="g++"
LIBS="-static -lm -mwindows -O3 -lcfgmgr32 -lwinmm -lsetupapi -lshlwapi -lcomctl32 -U_WIN64 -fpermissive"
#LIBS="-static -lm -mwindows -ggdb -lcfgmgr32 -lwinmm -lsetupapi -lshlwapi -lcomctl32 -U_WIN64 -fpermissive"
#LIBS="-static -lm -mwindows -fdiagnostics-color=always -g -lcfgmgr32 -lwinmm -lsetupapi -lshlwapi -lcomctl32 -U_WIN64 -fpermissive"
#LIBS="-static -fsanitize=address -lm -mwindows -ggdb -lcfgmgr32 -lwinmm -lsetupapi -lshlwapi -lcomctl32 -U_WIN64 -fpermissive"

#rm *.o *.exe *.a
rm *.o *.exe

#gcc -c esfm.c $LIBS -o esfm.o
#gcc -c esfm_registers.c $LIBS -o esfm_registers.o
#ar rcs libesfmu.a *.o
#rm *.o

# intermediate compile
$CC -c esdev.c $LIBS -o esdev.o
$CC -c esfmbank.c $LIBS -o esfmbank.o
$CC -c esfm.c $LIBS -o esfmu.o
$CC -c esfmu_helper.c $LIBS -o esfmu_helper.o
$CC -c esfm_registers.c $LIBS -o esfm_registers.o
$CC -c essplaymid/esfm.c $LIBS -o esfm.o
$CC -c essplaymid/natv.c $LIBS -o natv.o
$CC -c essplaymid/iodriver/drv_inpout32.c $LIBS -o drv_inpout32.o
$CC -c essplaymid/iodriver/InpOut32Helper.cpp $LIBS -o InpOut32Helper.o


# gui shit
windres.exe res.rc -o resource.o

# link
#g++ libesfmu.a *.o $LIBS -o test.exe
$CC *.o $LIBS -o test.exe
