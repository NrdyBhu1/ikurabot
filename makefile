# Makefile for Orion-X3/Orion-X4/mx and derivatives
# Written in 2011
# This makefile is licensed under the WTFPL


WARNINGS        = -Wno-padded -Wno-cast-align -Wno-unreachable-code -Wno-packed -Wno-missing-noreturn -Wno-float-equal -Wno-unused-macros -Werror=return-type -Wextra -Wno-unused-parameter -Wno-trigraphs

COMMON_CFLAGS   = -Wall -O3 -g

CC              := clang
CXX             := clang++

CFLAGS          = $(COMMON_CFLAGS) -std=c99 -fPIC -O3
CXXFLAGS        = $(COMMON_CFLAGS) -Wno-old-style-cast -std=c++17 -fno-exceptions -pthread

CXXSRC          = $(shell find source -iname "*.cpp" -print)
CXXOBJ          = $(CXXSRC:.cpp=.cpp.o)
CXXDEPS         = $(CXXOBJ:.o=.d)

PRECOMP_HDRS    := source/include/precompile.h
PRECOMP_GCH     := $(PRECOMP_HDRS:.h=.h.gch)

UTF8PROC_SRC    = external/utf8proc/utf8proc.c
UTF8PROC_OBJ    = $(UTF8PROC_SRC:.c=.c.o)
UTF8PROC_DEPS   = $(UTF8PROC_OBJ:.o=.d)

DEFINES         = -DKISSNET_NO_EXCEP -DKISSNET_USE_OPENSSL
INCLUDES        = $(shell pkg-config --cflags openssl) -Isource/include -Iexternal

.PHONY: all clean build
.PRECIOUS: $(PRECOMP_GCH)
.DEFAULT_GOAL = all

all: build
	@build/ikurabot build/config.json build/database.db --create

build: build/ikurabot

build/ikurabot: $(CXXOBJ) $(UTF8PROC_OBJ)
	@echo "  linking..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(shell pkg-config --libs openssl)

%.cpp.o: %.cpp makefile $(PRECOMP_GCH)
	@echo "  $(notdir $<)"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) $(INCLUDES) $(DEFINES) -include source/include/precompile.h -MMD -MP -c -o $@ $<

%.c.o: %.c makefile
	@echo "  $(notdir $<)"
	@$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

%.h.gch: %.h makefile
	@printf "# precompiling header $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) $(INCLUDES) -x c++-header -o $@ $<

clean:
	-@find source -iname "*.cpp.d" | xargs rm
	-@find source -iname "*.cpp.o" | xargs rm
	-@rm $(PRECOMP_GCH)

-include $(CXXDEPS)
-include $(CDEPS)












