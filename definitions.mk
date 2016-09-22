# common definitions

.PHONY: clean build libdir

# to be used below
PROJECT := OpenLTFS

# for linking - no standard C required
CC = g++

CXX = g++

# use c++11 to build the code
# CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -Wno-format-security -D_GNU_SOURCE -I$(RELPATH)
CXXFLAGS  := -std=c++11 -g2 -ggdb -Wall -Werror -D_GNU_SOURCE -I$(RELPATH) -I/opt/local/include -I/usr/local/include

# for protocol buffers
LDFLAGS := -lprotobuf -lpthread -L/usr/local/lib

BINDIR := $(RELPATH)/bin
LIBDIR := $(RELPATH)/lib

# client, common, or server
TARGETCOMP := $(shell perl -e "print '$(CURDIR)' =~ /.*$(PROJECT)\/src\/([^\/]+)/")

# to not set ARCHIVES (e.g. link w/o) set 'ARCHIVES :=' before
ARCHIVES ?= $(RELPATH)/lib/common.a $(RELPATH)/lib/$(TARGETCOMP).a

# library source files will be added to the archives
SOURCE_FILES := $(LIB_SRC_FILES)

DEPDIR := .d

ifneq ($(strip $(SOURCE_FILES)),)
DEPS := $(DEPDIR)/deps.mk
endif

ifneq ($(strip $(LIB_SRC_FILES)),)
TARGETLIB := $(LIBDIR)/$(TARGETCOMP).a
endif

TARGET := $(addprefix $(BINDIR)/, $(BINARY))

objfiles = $(patsubst %.cc,%.o, $(1))

# build rules
default: build

# creating diretories if missing
$(DEPDIR) $(LIBDIR) $(BINDIR):
	mkdir $@

# auto dependencies
$(DEPS): $(SOURCE_FILES) | $(DEPDIR)
	$(CXX) $(CXXFLAGS) -MM $(SOURCE_FILES) > $@

libdir: | $(LIBDIR)

# client.a, common.a, and server.a archive files
$(TARGETLIB): libdir $(TARGETLIB)($(call objfiles, $(LIB_SRC_FILES)))

# link binaries
$(BINARY): $(ARCHIVES)

# copy binary to bin directory
$(TARGET): $(BINARY) | $(BINDIR)
	cp $(BINARY) $(BINDIR)

clean:
	rm -fr $(RELPATH)/lib/* *.o $(CLEANUP_FILES) $(BINARY) $(BINDIR)/* $(DEPDIR)

build: $(DEPS) $(call objfiles, $(SOURCE_FILES)) $(TARGETLIB) $(TARGET) $(POSTTARGET)

ifeq ($(MAKECMDGOALS),build)
	-include .d/deps.mk
endif
