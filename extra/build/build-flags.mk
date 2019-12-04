include $(ROOTDIR)/config.mk

ifeq ($(CONFIG_HAVE_DEBUG),y)
CFLAGS      += -g -Og -fno-omit-frame-pointer -Wall
CXXFLAGS    += -g -Og -fno-omit-frame-pointer -Wall
ASFLAGS     += -g -Og -fno-omit-frame-pointer -Wall
DEFINES     += DEBUG=1
else
CFLAGS      += -O3
CXXFLAGS    += -O3
ASFLAGS     += -O3
LDFLAGS     += -s
DEFINES     += NDEBUG=1
endif


CFLAGS	    += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES))
CXXFLAGS    += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES))
ASFLAGS     += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) -D__ASSEMBLY__

CFLAGS	    += $(subst $\",,$(CONFIG_COMPILER_EXTRA_CFLAGS))
CXXFLAGS    += $(subst $\",,$(CONFIG_COMPILER_EXTRA_CXXFLAGS))
ASFLAGS     += $(subst $\",,$(CONFIG_COMPILER_EXTRA_ASFLAGS))
LDFLAGS     += $(subst $\",,$(CONFIG_COMPILER_EXTRA_LDFLAGS))