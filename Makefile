# Detect OS and Architecture
OS := $(shell uname -s)
ifeq ($(findstring CYGWIN, $(OS)),CYGWIN)
    OS := Windows
endif

# Check for external gsl repository needed on Windows
ifeq ($(OS), Windows)
  GSL_REPO ?= $(firstword $(wildcard ../gsl ../../gsl))
  ifeq ($(GSL_REPO),)
    $(error GSL source code not found. Run 'git clone https://github.com/rtsoliday/gsl.git' next to the medm repository)
  endif
else
  GSL_REPO :=
endif

# Check for external SDDS repository
SDDS_SEARCH_PATHS := ../SDDS ../../../../epics/extensions/src/SDDS \
                     $(HOME)/SDDS $(HOME)/epics/extensions/src/SDDS \
                     /cygdrive/*/SDDS /cygdrive/*/epics/extensions/src/SDDS
SDDS_REPO ?= $(firstword $(wildcard $(SDDS_SEARCH_PATHS)))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the medm repository)
endif

ifneq ($(OS), Windows)
  GSL_LOCAL ?= $(wildcard $(SDDS_REPO)/gsl)
endif

ifneq ($(strip $(GSL_REPO)),)
  GSL_REPO := $(abspath $(GSL_REPO))
endif
ifneq ($(strip $(GSL_LOCAL)),)
  GSL_LOCAL := $(abspath $(GSL_LOCAL))
endif
SDDS_REPO := $(abspath $(SDDS_REPO))

export GSL_REPO
export GSL_LOCAL
export SDDS_REPO

include Makefile.rules
export MOTIF
export HAVE_MOTIF
export MOTIF_INC
export X11_INC
export HAVE_QT

DIRS :=
ifneq ($(strip $(GSL_REPO)),)
  DIRS += $(GSL_REPO)
endif
ifneq ($(strip $(GSL_LOCAL)),)
  DIRS += $(GSL_LOCAL)
endif
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
ifneq ($(OS), Windows)
  ifeq ($(HAVE_MOTIF), 1)
    DIRS += printUtils
    DIRS += xc
    DIRS += medm
  endif
endif
ifeq ($(HAVE_QT), 1)
  DIRS += qtedm
endif

.PHONY: all $(DIRS) clean distclean check-motif check-qt final-check

all: check-motif $(DIRS) final-check

# Check and inform user about Motif availability
check-motif:
ifneq ($(OS), Windows)
  ifneq ($(HAVE_MOTIF), 1)
	@echo ""
	@echo "=========================================="
	@echo "NOTE: Motif development libraries not found."
	@echo "  Missing library: $(if $(XM_LIB),,libXm)"
	@echo "  Missing headers: $(if $(MOTIF_INC),,Xm/Xm.h)"
	@echo "Skipping build of medm (Motif-based MEDM)."
	@echo "Only building qtedm (Qt-based EDM)."
	@echo ""
	@echo "To build medm, install Motif development packages:"
	@echo "  Debian/Ubuntu: sudo apt-get install libmotif-dev libxmu-dev"
	@echo "  RHEL/CentOS:   sudo yum install motif-devel libXmu-devel"
	@echo "  macOS:         brew install openmotif"
	@echo "=========================================="
	@echo ""
  endif
endif

# Legacy target - kept for compatibility
check-qt:

# Final check after building - report Qt status and exit if missing
final-check:
ifneq ($(HAVE_QT), 1)
	@echo ""
	@echo "=========================================="
	@echo "ERROR: Qt5 development libraries not found."
	@echo ""
	@echo "Qt5 is required to build qtedm."
	@echo ""
	@echo "To install Qt5 development packages:"
	@echo "  Debian/Ubuntu: sudo apt-get install qtbase5-dev qt5-qmake"
	@echo "  RHEL/CentOS:   sudo yum install qt5-qtbase-devel"
	@echo "  macOS:         brew install qt@5"
	@echo "=========================================="
	@echo ""
	@exit 1
endif

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
ifeq ($(HAVE_MOTIF), 1)
printUtils: $(SDDS_REPO)/mdbcommon
	$(MAKE) -C $@
xc: printUtils
	$(MAKE) -C $@
medm: xc
	$(MAKE) -C $@
endif
ifeq ($(HAVE_QT), 1)
qtedm: $(SDDS_REPO)/mdbcommon
	$(MAKE) -C $@
endif

clean:
ifeq ($(HAVE_MOTIF), 1)
	$(MAKE) -C printUtils clean
	$(MAKE) -C xc clean
	$(MAKE) -C medm clean
endif
ifeq ($(HAVE_QT), 1)
	$(MAKE) -C qtedm clean
endif
	
distclean: clean
	rm -rf bin/$(OS)-$(ARCH)
	rm -rf lib/$(OS)-$(ARCH)
