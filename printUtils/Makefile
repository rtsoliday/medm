include ../Makefile.rules

# Preserve the existing SDDS setup for Windows support-library builds.
ifeq ($(OS),Windows)
SDDS_SEARCH_PATHS := ../../SDDS ../../../../epics/extensions/src/SDDS \
                     $(HOME)/SDDS $(HOME)/epics/extensions/src/SDDS \
                     /cygdrive/*/SDDS /cygdrive/*/epics/extensions/src/SDDS
SDDS_REPO ?= $(firstword $(wildcard $(SDDS_SEARCH_PATHS)))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the medm repository)
endif
SDDS_REPO := $(abspath $(SDDS_REPO))
endif

LIBRARY = PrintUtils

LIBRARY_SRC = getopt.c pUtils.c ps_utils.c utilPrint.c xwd.c xwd2ps.c

CFLAGS += $(if $(filter Windows,$(OS)),-I$(SDDS_REPO)/include) -I$(OBJ_DIR)
override CXXFLAGS += $(if $(filter Windows,$(OS)),-I$(SDDS_REPO)/include) -I$(OBJ_DIR)

ifeq ($(OS), Linux)
  LDFLAGS := -fopenmp $(LDFLAGS)
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LINK_LIBS) \
    $(Z_LIB) $(PROD_SYS_LIBS)
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LINK_LIBS) $(Z_LIB) $(PROD_SYS_LIBS)
endif

ifeq ($(OS), Windows)
  NO_DLL = 1
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(SDDS_REPO)/lzma -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
  override CXXFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(SDDS_REPO)/lzma -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
  PROD_LIBS += $(GSL_LINK_LIBS)  ws2_32.lib
  PROD_LIBS_SDDS = mdbcommon.lib matlib.lib SDDS1.lib namelist.lib rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib
  LIB_LINK_DIRS += -LIBPATH:$(SDDS_REPO)/lib/$(OS)-$(ARCH)
endif

include ../Makefile.build
