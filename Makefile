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

ifneq ($(strip $(GSL_REPO)),)
  GSL_REPO := $(abspath $(GSL_REPO))
endif

export GSL_REPO

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

.PHONY: all $(DIRS) clean distclean check-motif check-qt final-check \
        test tests test-qtedm test-qtedm-cli test-qtedm-unit \
        test-qtedm-ioc test-qtedm-visual

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
	@echo "ERROR: Qt development libraries not found."
	@echo ""
	@echo "Qt5 or Qt6 is required to build qtedm."
	@echo ""
	@echo "To install Qt development packages:"
	@echo "  Debian/Ubuntu: sudo apt-get install qtbase5-dev qt5-qmake"
	@echo "                 or sudo apt-get install qt6-base-dev"
	@echo "  RHEL/CentOS:   sudo yum install qt5-qtbase-devel"
	@echo "                 or sudo yum install qt6-qtbase-devel"
	@echo "  macOS:         brew install qt@5"
	@echo "                 or brew install qt"
	@echo "=========================================="
	@echo ""
	@exit 1
endif

ifneq ($(GSL_REPO),)
  $(GSL_REPO):
	$(MAKE) -C $@ -f Makefile.MSVC all
endif
ifeq ($(HAVE_MOTIF), 1)
printUtils: $(GSL_REPO)
	$(MAKE) -C $@
xc: printUtils
	$(MAKE) -C $@
medm: xc
	$(MAKE) -C $@
endif
ifeq ($(HAVE_QT), 1)
qtedm: $(GSL_REPO)
	$(MAKE) -C $@
endif

ifeq ($(HAVE_QT), 1)
test: test-qtedm

tests: test

test-qtedm: qtedm
	$(MAKE) -C qtedm test

test-qtedm-cli: qtedm
	$(MAKE) -C qtedm test-cli

test-qtedm-unit: qtedm
	$(MAKE) -C qtedm test-unit

test-qtedm-ioc: qtedm
	$(MAKE) -C qtedm test-ioc

test-qtedm-visual: qtedm
	$(MAKE) -C qtedm test-visual
else
test tests test-qtedm test-qtedm-cli test-qtedm-unit test-qtedm-ioc test-qtedm-visual:
	@echo "Qt development libraries are required to run qtedm tests."
	@exit 1
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
