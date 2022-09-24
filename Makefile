V := @
SRCS := $(wildcard ./*.c)
OBJS := $(patsubst %.c,%.o, $(SRCS))
TARGETS := $(patsubst %.c,%, $(SRCS))

.PHONY: all clean

CC := gcc
CFLAGS := -Wall -g -I./

all: $(TARGETS)

%: %.c
	@echo CC $@
	$(V) $(CC) -o $@ $< $(CFLAGS) -ldl

clean:
	rm -fr $(OBJS) $(TARGETS) *.txt *.bin

