.PHONY: all
.SUFFIXES: .java .class

CC			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/sys/apps/$(notdir $(PWD)).app

JFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.java")
CLASSFILES	:=	$(JFILES:.java=.class)



all: $(OUTPUT)
$(OUTPUT): $(CLASSFILES)
	@mkdir -p $(PWD)/res
	@echo "  JAR    " $(subst $(ROOT)/,,$@)
	@jar -cf $(OUTPUT) $(subst $(PWD)/,,$(CLASSFILES)) res


.java.class:
	@echo "  JAVA   " $(subst $(ROOT)/,,$@)
	@javac -classpath . -classpath $(ROOT)/bin/usr/lib/avm/zeta.jar $<

	
clean:
	$(RM) $(CLASSFILES) $(OUTPUT)