# Detect OS and Architecture
OS := $(shell uname -s)
ifeq ($(findstring CYGWIN, $(OS)),CYGWIN)
    OS := Windows
endif

# Check for external gsl repository needed on Windows
ifeq ($(OS), Windows)
  GSL_REPO = $(wildcard ../gsl)
  ifeq ($(GSL_REPO),)
    $(error GSL source code not found. Run 'git clone https://github.com/rtsoliday/gsl.git' next to the adt repository)
  endif
endif

# Check for external SDDS repository
SDDS_REPO = $(firstword $(wildcard ../SDDS ../../../../epics/extensions/src/SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the adt repository)
endif

ifeq ($(OS), Linux)
  GSL_LOCAL = $(wildcard $(SDDS_REPO)/gsl)
endif

include Makefile.rules
export MOTIF

DIRS = $(GSL_REPO)
DIRS += $(GSL_LOCAL)
DIRS += $(SDDS_REPO)/include
DIRS += $(SDDS_REPO)/zlib
DIRS += $(SDDS_REPO)/lzma
DIRS += $(SDDS_REPO)/mdblib
DIRS += $(SDDS_REPO)/mdbmth
DIRS += $(SDDS_REPO)/rpns/code
DIRS += $(SDDS_REPO)/namelist
DIRS += $(SDDS_REPO)/SDDSlib
DIRS += $(SDDS_REPO)/fftpack
DIRS += $(SDDS_REPO)/matlib
DIRS += $(SDDS_REPO)/mdbcommon
DIRS += printUtils
DIRS += xc 
DIRS += medm

.PHONY: all $(DIRS) clean distclean

all: $(DIRS)

ifneq ($(GSL_REPO),)
  $(GSL_REPO):
	$(MAKE) -C $@ -f Makefile.MSVC all
endif
ifneq ($(GSL_LOCAL),)
  $(GSL_LOCAL):
	$(MAKE) -C $@ all
endif
$(SDDS_REPO)/include: $(GSL_REPO) $(GSL_LOCAL)
	$(MAKE) -C $@
$(SDDS_REPO)/zlib: $(SDDS_REPO)/include
	$(MAKE) -C $@
$(SDDS_REPO)/lzma: $(SDDS_REPO)/zlib
	$(MAKE) -C $@
$(SDDS_REPO)/mdblib: $(SDDS_REPO)/lzma
	$(MAKE) -C $@
$(SDDS_REPO)/mdbmth: $(SDDS_REPO)/mdblib
	$(MAKE) -C $@
$(SDDS_REPO)/rpns/code: $(SDDS_REPO)/mdbmth $(GSL_REPO) $(GSL_LOCAL)
	$(MAKE) -C $@
$(SDDS_REPO)/namelist: $(SDDS_REPO)/rpns/code
	$(MAKE) -C $@
$(SDDS_REPO)/SDDSlib: $(SDDS_REPO)/namelist
	$(MAKE) -C $@
$(SDDS_REPO)/fftpack: $(SDDS_REPO)/SDDSlib
	$(MAKE) -C $@
$(SDDS_REPO)/matlib: $(SDDS_REPO)/fftpack
	$(MAKE) -C $@
$(SDDS_REPO)/mdbcommon: $(SDDS_REPO)/matlib
	$(MAKE) -C $@
printUtils: $(SDDS_REPO)/mdbcommon
	$(MAKE) -C $@
xc: printUtils
	$(MAKE) -C $@
medm: xc
	$(MAKE) -C $@

clean:
	$(MAKE) -C printUtils clean
	$(MAKE) -C xc clean
	$(MAKE) -C medm clean

distclean: clean
	rm -rf bin/$(OS)-$(ARCH)
	rm -rf lib/$(OS)-$(ARCH)
