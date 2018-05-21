AUTOMAKE_OPTIONS = cygnus
INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)
AM_CCASFLAGS = $(INCLUDES)
 
noinst_LIBRARIES = lib.a
 
if MAY_SUPPLY_SYSCALLS
extra_objs = syscalls.o pipe.o gethostname.o system.o geteuid.o getegid.o \
		access.o sleep.o usleep.o killpg.o dup2.o pgrp.o \
		sigsuspend.o sigaction.o tcpgrp.o utime.o rmdir.o mkdir.o \
		vfork.o netdb.o exit.o
else
extra_objs =
endif
 
lib_a_SOURCES =
lib_a_LIBADD = $(extra_objs)
EXTRA_lib_a_SOURCES = gethostname.c pipe.c syscalls.c system.c geteuid.c getegid.c \
			 access.c sleep.c usleep.c killpg.c dup2.c pgrp.c \
			 sigsuspend.c sigaction.c tcpgrp.o rmdir.c mkdir.c utime.c \
			vfork.c netdb.c exit.c \
			crt0.c
lib_a_DEPENDENCIES = $(extra_objs)
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)
 
if MAY_SUPPLY_SYSCALLS
all: crt0.o
endif
 
ACLOCAL_AMFLAGS = -I ../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
