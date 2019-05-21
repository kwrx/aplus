LOCAL_DIR := .

include $(PROJECT_ROOT)/build/project.mk
include $(PROJECT_ROOT)/build/target/$(TARGET).mk
include $(PROJECT_ROOT)/build/toolchain.mk
-include $(PROJECT_ROOT)/build/toolchain-config.mk


PROJECT_OUTPUT_H :=  \
	$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)/include/vterm.h	\
	$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)/include/vterm_keycodes.h

PROJECT_OUTPUT_LIB :=  \
	$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)/lib/lib$(notdir $(PWD)).a


PROJECT_HDRS += \
	$(LOCAL_DIR)/include/vterm.h							\
	$(LOCAL_DIR)/include/vterm_keycodes.h


PROJECT_SRCS += \
	$(shell find $(LOCAL_DIR) -type f -name "*.c")			\
	$(shell find $(LOCAL_DIR) -type f -name "*.cpp")		\
	$(shell find $(LOCAL_DIR) -type f -name "*.S")			\
	$(shell find $(LOCAL_DIR) -type f -name "*.asm")


PROJECT_GCFLAGS +=  \
	-fpic
	
PROJECT_DEFINES := #Nothing

	

all: $(PROJECT_OUTPUT_LIB) $(PROJECT_OUTPUT_H)
clean: $(PROJECT_CLEAN)


include $(PROJECT_ROOT)/build/compile.mk