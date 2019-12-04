include $(ROOTDIR)/config.mk

CC		:= $(subst $\",,$(CONFIG_COMPILER_HOST))-aplus-gcc
CXX 	:= $(subst $\",,$(CONFIG_COMPILER_HOST))-aplus-g++
LD		:= $(subst $\",,$(CONFIG_COMPILER_HOST))-aplus-gcc
AS		:= $(subst $\",,$(CONFIG_COMPILER_HOST))-aplus-gcc
AR		:= $(subst $\",,$(CONFIG_COMPILER_HOST))-aplus-ar