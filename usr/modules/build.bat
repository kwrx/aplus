@echo off

set curdir=%cd%
set bindir=%curdir%\bin
set srcdir=%curdir%\src
set rddir=%curdir%\..\..\initrd

set CC=i586-elf-gcc
set CFLAGS=-O2 -I include -I ../../../../src/include  -c

set ASM=nasm
set AFLAGS=-f elf

set CXX=i586-elf-g++
set CXXFLAGS=-O2 -I include -c


set LDIR=/usr/cross/i586-elf/lib
set LD=i586-elf-ld
set LFLAGS=-static -T %LDIR%/ldscripts/elf_i386.x
set LIBS= -lx86 -lz -lm -lc -lgcc


cd %srcdir%

for /f %%d in ('dir /a:d /b') do (

	cd %%d
	echo %%d
	
	for %%f in (*.asm) do (
		echo   ASM	%%f
		%ASM% %AFLAGS% %%f -o %%f.o
	)
		
	for %%f in (*.c) do (
		echo   CC	%%f
		%CC% %CFLAGS% %%f -o %%f.o
	)

	for %%f in (*.cpp) do (
		echo   CXX	%%f
		%CXX% %CXXFLAGS% %%f -o %%f.o
	)
	
	%LD% %LFLAGS% -o %%d.elf %LDIR%/crt0.o *.o %LIBS%
	copy %%d.elf %bindir%\%%d.km > nul
	
	del /Q /F *.o > nul
	
	cd %srcdir%
)

cd %bindir%
copy * %rddir%\*

cd %curdir%
