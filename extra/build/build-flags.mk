include $(ROOTDIR)/config.mk

ifeq ($(CONFIG_HAVE_DEBUG),y)

	CFLAGS      += -g -Og -fno-omit-frame-pointer -Wall -Werror
	CXXFLAGS    += -g -Og -fno-omit-frame-pointer -Wall -Werror
	ASFLAGS     += -g -Og -fno-omit-frame-pointer -Wall -Werror
	DEFINES     += DEBUG=1


	ifeq ($(CONFIG_HAVE_SANITIZERS),y)
		ifneq (,$(findstring KERNEL=1,$(DEFINES)))

			SANITIZERS := 					\
				shift						\
				shift-exponent  			\
				shift-base					\
				integer-divide-by-zero		\
				unreachable					\
				null						\
				return						\
				signed-integer-overflow		\
				bounds						\
				bounds-strict				\
				object-size					\
				float-divide-by-zero		\
				float-cast-overflow			\
				nonnull-attribute			\
				returns-nonnull-attribute	\
				bool						\
				enum						\
				vptr				    	\
				pointer-overflow			\
				builtin

			CFLAGS   	+= -fstack-protector-explicit $(addprefix -fsanitize=,$(SANITIZERS))
			CXXFLAGS 	+= -fstack-protector-explicit $(addprefix -fsanitize=,$(SANITIZERS))

		endif
	else
		ifneq (,$(findstring KERNEL=1,$(DEFINES)))
			CFLAGS   	+= -fno-stack-protector
			CXXFLAGS 	+= -fno-stack-protector
		endif
	endif

else

	CFLAGS      += -O$(CONFIG_COMPILER_OPTIMIZATION_LEVEL)
	CXXFLAGS    += -O$(CONFIG_COMPILER_OPTIMIZATION_LEVEL)
	ASFLAGS     += -O$(CONFIG_COMPILER_OPTIMIZATION_LEVEL)
	DEFINES     += NDEBUG=1

	ifeq ($(CONFIG_COMPILER_STRIP_BINARIES),y)
		LDFLAGS     += -Wl,--strip-debug
	endif

endif

ifneq (,$(findstring KERNEL=1,$(DEFINES)))

	CFLAGS   += -fno-builtin -mgeneral-regs-only
	CXXFLAGS += -fno-builtin -mgeneral-regs-only
	ASFLAGS  += -fno-builtin -mgeneral-regs-only

endif


CFLAGS	    += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES))
CXXFLAGS    += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES))
ASFLAGS     += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) -D__ASSEMBLY__

CFLAGS	    += $(subst $\",,$(CONFIG_COMPILER_EXTRA_CFLAGS))
CXXFLAGS    += $(subst $\",,$(CONFIG_COMPILER_EXTRA_CXXFLAGS))
ASFLAGS     += $(subst $\",,$(CONFIG_COMPILER_EXTRA_ASFLAGS))
LDFLAGS     += $(subst $\",,$(CONFIG_COMPILER_EXTRA_LDFLAGS))