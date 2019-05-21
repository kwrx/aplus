LOCAL_DIR := .

include $(PROJECT_ROOT)/build/project.mk
include $(PROJECT_ROOT)/build/target/$(TARGET).mk
include $(PROJECT_ROOT)/build/toolchain.mk
-include $(PROJECT_ROOT)/build/toolchain-config.mk



PROJECT_OUTPUT_BIN := \
	$(LOCAL_DIR)/ap.h

PROJECT_SRCS := \
	$(LOCAL_DIR)/ap.asm


all: $(PROJECT_OUTPUT_BIN)
$(PROJECT_OUTPUT_BIN): $(PROJECT_SRCS)
	@echo "  GEN    " $@
	$(NOECHO)$(PROJECT_ROOT)/extra/utils/gen-ap $@ $< $(ASM)

clean:
	$(RM) $(PROJECT_OUTPUT_BIN)

include $(PROJECT_ROOT)/build/compile.mk