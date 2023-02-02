SRCS        ?=  $(shell find $(SRCDIRS) -type f -name "*.S")                \
                $(shell find $(SRCDIRS) -type f -name "*.s")                \
                $(shell find $(SRCDIRS) -type f -name "*.cxx")              \
                $(shell find $(SRCDIRS) -type f -name "*.cpp")              \
                $(shell find $(SRCDIRS) -type f -name "*.cc")               \
                $(shell find $(SRCDIRS) -type f -name "*.c")                \
                $(shell find $(SRCDIRS) -type f -name "*.asm")

HDRS	    ?=  $(shell find $(SRCDIRS) -type f -name "*.h")                \
                $(shell find $(SRCDIRS) -type f -name "*.hpp")

OBJS        :=  $(addsuffix .o,$(basename $(SRCS)))
DEPS        :=  $(addsuffix .d,$(basename $(SRCS)))

CARGO_TOML  ?=  $(shell find $(SRCDIRS) -type f -name "*.toml")
CARGO_OBJS  :=  $(addsuffix .a,$(basename $(CARGO_TOML)))

RESOURCES   ?=