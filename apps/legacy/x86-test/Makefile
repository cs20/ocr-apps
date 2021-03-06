#
# Makefile
# vim:syntax=make noexpandtab:
#
ARCH=x86-test

TG_INSTALL ?= $(realpath $(CURDIR)/../../../../tg/tg/install)
APPS_ROOT  ?= $(realpath $(CURDIR)/../..)
export APPS_LIBS_INSTALL?=$(APPS_ROOT)/libs/install/$(ARCH)

LIBS = $(APPS_LIBS_INSTALL)/lib
OCR_INC = $(APPS_LIBS_INSTALL)/include

TARGET = x86_64-linux-elf
TOOLBIN = $(TG_INSTALL)/bin
TOOL_PREFIX = $(TOOLBIN)/$(TARGET)-
CC = $(TOOL_PREFIX)clang
CXX = $(TOOL_PREFIX)clang++
LD = $(TOOL_PREFIX)ld

# Libraries to include from newlib
NL_CRT = $(LIBS)/crt0.o
NL_LIBS = $(LIBS)/libc.a $(LIBS)/libm.a
CPP_LIBS = $(LIBS)/libc++.a $(LIBS)/libc++abi.a $(LIBS)/libunwind.a
NL_CRTEND = $(LIBS)/crtend.o

# Other library - this uses the libscaffold.a, but it
# should actually be the real ocr library/runtime.
OCR_LIB = $(LIBS)/libscaffold.a

CAPP = hello
CAPP_SRCS = hello.c
CAPP_LIBS =
CAPP_OBJS = $(CAPP_SRCS:.c=.o)
CFLAGS = -Os -nostdinc -isystem $(OCR_INC)
CFLAGS += -isystem $(TG_INSTALL)/lib/clang/3.9.0/include
CFLAGS += -isystem $(TG_INSTALL)/include
CFLAGS += -isystem $(TG_INSTALL)/include/x86_64-linux-elf

CXXAPP = cxxhello
CXXAPP_SRCS = cxxhello.cpp
CXXAPP_LIBS =
CXXAPP_OBJS = $(CXXAPP_SRCS:.cpp=.o)
CXXFLAGS = -Os -nostdinc -isystem $(OCR_INC)
CXXFLAGS += -isystem $(APPS_LIBS_INSTALL)/include/c++/v1
CXXFLAGS += -isystem $(APPS_ROOT)/libs/src/libunwind/libunwind/include
CXXFLAGS += -isystem $(TG_INSTALL)/lib/clang/3.9.0/include
CXXFLAGS += -isystem $(TG_INSTALL)/include
CXXFLAGS += -isystem $(TG_INSTALL)/include/x86_64-linux-elf

all: $(CAPP) $(CXXAPP)

default: all

$(CAPP) : $(CAPP_OBJS)
	@$(LD) -o $@ $(NL_CRT) $< $(NL_LIBS) $(OCR_LIB) $(CAPP_LIBS) $(NL_CRTEND)
	@echo Successfully built $@

$(CXXAPP) : $(CXXAPP_OBJS)
	@$(LD) -o $@ $(NL_CRT) $< $(CPP_LIBS) $(NL_LIBS) $(OCR_LIB) $(CXXAPP_LIBS) $(NL_CRTEND)
	@echo Successfully built $@

%.o : %.c
	@$(CC) -c $(CFLAGS) $< -o $@

%.o : %.cpp
	@$(CXX) -c $(CXXFLAGS) $< -o $@

dump:
	@echo TG_INSTALL = $(TG_INSTALL)
	@echo APPS_ROOT = $(APPS_ROOT)
	@echo LIBS = $(LIBS)

clean:
	@rm -f $(CAPP) $(CAPP_OBJS) $(CXXAPP) $(CXXAPP_OBJS)
