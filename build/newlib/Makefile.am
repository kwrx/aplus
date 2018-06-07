AUTOMAKE_OPTIONS = cygnus
INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)
AM_CCASFLAGS = $(INCLUDES)
 
noinst_LIBRARIES = lib.a
 
if MAY_SUPPLY_SYSCALLS
extra_objs = syscalls.o dup2.o exit.o gethostname.o			\
			 killpg.o mkdir.o netdb.o pipe.o rmdir.o		\
			 sigaction.o sigsuspend.o sleep.o system.o		\
			 tcpgrp.o usleep.o utime.o gettimeofday.o		\
			 wait.o access.o set_getpgrp.o sigwaitinfo.o
else
extra_objs =
endif
 
lib_a_SOURCES =
lib_a_LIBADD = $(extra_objs)

EXTRA_lib_a_SOURCES = crt0.c syscalls.c dup2.c exit.c 		\
					  gethostname.c gettimeofday.c			\
					  killpg.c mkdir.c netdb.c pipe.c		\
					  rmdir.c sigaction.c sigsuspend.c		\
					  sleep.c system.c tcpgrp.c usleep.c 	\
					  utime.c wait.c access.c set_getpgrp.c	\
					  sigwaitinfo.c

lib_a_DEPENDENCIES = $(extra_objs)
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)
 
if MAY_SUPPLY_SYSCALLS
all: crt0.o
endif
 
ACLOCAL_AMFLAGS = -I ../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
