ifeq ($(wildcard config.mk),)
CONFIG := config.def.mk
else
include config.mk
CONFIG := config.mk
endif
include $(CONFIG)
DEFINES := $(shell grep '^[A-Z]' $(CONFIG) | sed 's/\s*[\t :]//g' | sed 's/^/-D/')

CFILES := rtcmain.c rtclib.c
# CFILE += ll.c hashtable.c
HFILES := rtclib.h ll.h hashtable.h common.h member.h
CC := gcc
# CC := clang -fsanitize=thread
# CC := clang -fsanitize=undefined
# CC := clang -fsanitize=memory
CFLAGS += -o rtc
CFLAGS += -Wall -Wextra
# CFLAGS += -fno-inline

ifdef PARALLEL
CFLAGS += -pthread
ifeq ($(PARALLEL),LACE)
CFLAGS += -I.
CFLAGS += -std=gnu11
else
CFLAGS += -std=c11
endif
endif

ifneq ($(DEBUG),)
CFLAGS += -O0 -g3
else
CFLAGS += -O3 -DNDEBUG
endif

ifeq ($(PARALLEL),CILK)
DEFINES += -DTHREADS=$(shell nproc)
endif

ifeq ($(PARALLEL),LACE)
DEFINES += -DTHREADS=$(shell nproc)
endif

CCARGS := $(CFLAGS) $(CFILES) $(DEFINES)

rtc: *.[ch] Makefile $(CONFIG)

ifeq ($(PARALLEL),CILK)
	source ./cilk.sh && $(CC) -fcilkplus -lcilkrts $(CCARGS)
else
	$(CC) $(CCARGS)
endif

SAMPLE_INPUT := ../benchmarks/sc14/app/preprocessed/MD5-29-2.cnf
SAMPLE_ARGS := $(SAMPLE_INPUT) preprocessed proof

valgrind: rtc
	valgrind --leak-check=full --quiet ./rtc $(SAMPLE_ARGS)
gdb: rtc
	gdb --args ./rtc $(SAMPLE_ARGS)
format:
	clang-format -i $(CFILES) $(HFILES)
tags: *.[ch]
	ctags *.[ch]
clean:
	rm -f rtc vgcore.* perf.data.*

.PHONY: clean
