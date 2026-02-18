# Root Makefile
# Default: build every */src/*.cpp (excluding libs/) into */bin/<name>
# and link with a static library built from libs/**/*.cpp when present.

SHELL := /usr/bin/env bash

CXX      ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -std=c++17 -O0 -Wall -Wextra
LDFLAGS  ?= -g
LDLIBS   ?=

# Common include paths (override/extend from command line if needed)
CPPFLAGS += -Ilibs -Ilibs/include

BUILD_DIR      := build
LIBS_BUILD_DIR := $(BUILD_DIR)/libs

# Top-level dirs (exclude libs/, build/ and hidden dirs)
TOP_DIRS  := $(filter-out libs/ build/ .vscode/,$(wildcard */))
TP_DIRS   := $(patsubst %/,%,$(TOP_DIRS))

TP_SRCS := $(foreach d,$(TP_DIRS),$(wildcard $(d)/src/*.cpp $(d)/src/*.cc $(d)/src/*.cxx))

# Map: tp1/src/test.cpp -> tp1/bin/test
# (Can't use patsubst with multiple '%' wildcards; use fixed /src/ replacement.)
BIN_FROM_SRC = $(subst /src/,/bin/,$(basename $(1)))
BINS := $(foreach s,$(TP_SRCS),$(call BIN_FROM_SRC,$(s)))

# libs sources (optional)
LIBS_SRCS := $(strip $(shell [ -d libs ] && find libs -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) 2>/dev/null || true))

ifneq ($(LIBS_SRCS),)
LIBS_OBJS := $(addsuffix .o,$(basename $(patsubst libs/%, $(LIBS_BUILD_DIR)/%, $(LIBS_SRCS))))
LIBS_ARCHIVE := $(BUILD_DIR)/liblibs.a
else
LIBS_OBJS :=
LIBS_ARCHIVE :=
endif

.PHONY: all libs clean distclean
all: libs $(BINS)

libs: $(LIBS_ARCHIVE)

# --- Build the libs static archive (only if there are sources) ---
ifneq ($(LIBS_ARCHIVE),)
$(LIBS_ARCHIVE): $(LIBS_OBJS)
	@mkdir -p $(@D)
	ar rcs $@ $(LIBS_OBJS)

$(LIBS_BUILD_DIR)/%.o: libs/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(LIBS_BUILD_DIR)/%.o: libs/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(LIBS_BUILD_DIR)/%.o: libs/%.cxx
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
endif


define MAKE_EXE_RULE
$$(call BIN_FROM_SRC,$(1)): $(1) $$(LIBS_ARCHIVE)
	@mkdir -p $$(@D)
	$$(CXX) $$(CPPFLAGS) $$(CXXFLAGS) $(1) -o $$@ $$(LDFLAGS) $$(LDLIBS) $$(LIBS_ARCHIVE)
endef

$(foreach s,$(TP_SRCS),$(eval $(call MAKE_EXE_RULE,$(s))))

clean:
	@rm -rf $(BUILD_DIR)

# Also removes compiled binaries in tp*/bin/
distclean: clean
	@rm -f $(BINS)
