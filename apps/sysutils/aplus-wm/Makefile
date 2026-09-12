INCLUDES += include
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include

# The sysroot headers arrive through -isystem rather than INCLUDES (which becomes -I):
# musl's endian.h trips -Wparentheses, and with -Werror on that turns a system header
# into a build failure as soon as its directory is named as a user include path.
# -include config.h has to be repeated here because build-binary.mk only supplies it
# with ?=, which any CFLAGS assignment above it would silence.
CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

LIBS     += ui cairo pixman-1 freetype png z

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
