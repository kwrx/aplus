INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include

# The sysroot headers arrive through -isystem rather than INCLUDES (which becomes -I):
# musl's endian.h trips -Wparentheses, and with -Werror on that turns a system header
# into a build failure as soon as its directory is named as a user include path.
# -include config.h has to be repeated here because build-binary.mk only supplies it
# with ?=, which any CFLAGS assignment above it would silence -- and this file is one
# big #if defined(CONFIG_HAVE_MESA), so losing it would compile the program away.
CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include

LIBS     += ui GL stdc++ supc++ z m

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
