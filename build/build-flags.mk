include $(ROOTDIR)/config.mk

ifeq ($(CONFIG_HAVE_DEBUG),y)

	CFLAGS      += -g3 -Og -fno-omit-frame-pointer -Wall -Werror
	CXXFLAGS    += -g3 -Og -fno-omit-frame-pointer -Wall -Werror
	ASFLAGS     += -g3 -Og -fno-omit-frame-pointer -Wall -Werror
	RUSTFLAGS   += -g
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

	CARGO_FLAGS +=
	CARGO_BUILD := debug

else

	CFLAGS      += -O$(CONFIG_COMPILER_OPTIMIZATION_LEVEL)
	CXXFLAGS    += -O$(CONFIG_COMPILER_OPTIMIZATION_LEVEL)
	ASFLAGS     += -O$(CONFIG_COMPILER_OPTIMIZATION_LEVEL)
	RUSTFLAGS   += -O$(CONFIG_COMPILER_OPTIMIZATION_LEVEL)
	DEFINES     += NDEBUG=1

	ifeq ($(CONFIG_COMPILER_STRIP_BINARIES),y)
		LDFLAGS     += -Wl,--strip-debug
	endif

	CARGO_FLAGS += --release
	CARGO_BUILD := release

endif

ifneq (,$(findstring KERNEL=1,$(DEFINES)))

	CFLAGS   += -fno-builtin -mgeneral-regs-only
	CXXFLAGS += -fno-builtin -mgeneral-regs-only -fno-rtti -fno-exceptions
	ASFLAGS  += -fno-builtin -mgeneral-regs-only

	RUSTFLAGS += -C panic=abort
	RUSTFLAGS += -C code-model=large
	RUSTFLAGS += -C relocation-model=static
	RUSTFLAGS += -C linker-flavor=ld.lld
	RUSTFLAGS += -C soft-float=y

endif



#
# Defines
#
CFLAGS	    += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES))
CXXFLAGS    += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES))
ASFLAGS     += $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) -D__ASSEMBLY__


#
# Standard
#
CFLAGS	  	+= -D_GNU_SOURCE -std=gnu17 
CXXFLAGS 	+= -D_GNU_SOURCE -std=gnu++17


#
# Extra
#
CFLAGS	    += $(subst $\",,$(CONFIG_COMPILER_EXTRA_CFLAGS))
CXXFLAGS    += $(subst $\",,$(CONFIG_COMPILER_EXTRA_CXXFLAGS))
ASFLAGS     += $(subst $\",,$(CONFIG_COMPILER_EXTRA_ASFLAGS))
LDFLAGS     += $(subst $\",,$(CONFIG_COMPILER_EXTRA_LDFLAGS))

#
# Cargo
#
CARGO_FLAGS += --target $(ROOTDIR)/build/rust/$(CARGO_TARGET).json

#
# Rust
#
RUSTFLAGS   += --target $(ROOTDIR)/build/rust/$(CARGO_TARGET).json