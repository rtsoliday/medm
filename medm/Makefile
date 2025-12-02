EPICS_SEARCH_PATHS := /usr/local/oag/base ../../epics-base ../../../../base \
                      $(HOME)/epics/base $(HOME)/epics/base-7.0 \
                      /cygdrive/*/epics/base /cygdrive/*/epics/base-7.0
EPICS_BASE ?= $(firstword $(wildcard $(EPICS_SEARCH_PATHS)))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the medm repository)
endif

EPICS_BASE := $(abspath $(EPICS_BASE))

# Detect if libCom.a was built with readline support by checking for undefined readline symbol
LIBCOM_PATH := $(EPICS_BASE)/lib/$(shell uname -s | sed 's/CYGWIN.*/windows-x64/' | sed 's/Darwin/darwin-x86/' | tr 'A-Z' 'a-z')-$(shell uname -m)/libCom.a
LIBCOM_NEEDS_READLINE := $(shell nm $(LIBCOM_PATH) 2>/dev/null | grep -q "U readline$$" && echo 1)

include ../Makefile.rules

PROD_LIBS :=

ifneq ($(OS), Windows)
  PROD_LIBS += -lANLwidgets -lPrintUtils
endif

PROD = medm

medm_SRC = medmCartesianPlot.c medmSciPlot.c SciPlot.c medmCA.c actions.c bubbleHelp.c callbacks.c browserHelp.c channelPalette.c colorPalette.c dialogs.c display.c eventHandlers.c help.c medm.c medmArc.c medmBar.c medmByte.c medmChoiceButtons.c medmCommon.c medmComposite.c medmControl.c medmDisplay.c medmImage.c medmIndicator.c medmMenu.c medmMessageButton.c medmMeter.c medmMonitor.c medmOval.c medmPixmap.c medmPolygon.c medmPolyline.c medmRectangle.c medmRelatedDisplay.c medmShellCommand.c medmStripChart.c medmText.c medmTextEntry.c medmTextUpdate.c medmValuator.c medmWheelSwitch.c medmWidget.c objectPalette.c productDescriptionShell.c resourcePalette.c updateMonitors.c updateTask.c utils.c xgif.c

CFLAGS += -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc -DSCIPLOT -DMOTIF -DEDITRES
CCFLAGS += -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc -DSCIPLOT -DMOTIF -DEDITRES
CFLAGS += -I$(MOTIF_INC) -I$(X11_INC)
CCFLAGS += -I$(MOTIF_INC) -I$(X11_INC)

ifeq ($(OS), Linux)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  CCFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) \
    $(Z_LIB) $(if $(LIBCOM_NEEDS_READLINE),-lreadline) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS += $(XM_LIB) $(XMU_LIB) $(XT_LIB) $(X11_LIB) $(XEXT_LIB)
endif

ifeq ($(OS), Darwin)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-deprecated-builtins
  CCFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS += $(XM_LIB) $(XMU_LIB) $(XT_LIB) $(X11_LIB) $(XEXT_LIB)
  PROD_SYS_LIBS += $(XFT_LIB) $(FONTCONFIG_LIB) $(SM_LIB) $(ICE_LIB) $(XP_LIB) $(XAU_LIB) $(JPEG_LIB) $(PNG_LIB) $(ICONV_LIB)
endif

ifeq ($(OS), Windows)
  NO_DLL = 1
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  CCFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  PROD_LIBS += ANLwidgets.lib PrintUtils.lib gsl.lib gslcblas.lib ws2_32.lib \
               pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib
  LIB_LINK_DIRS += -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
endif

include ../Makefile.build

$(OBJ_DIR)/medm$(EXEEXT): $(medm_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(medm_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
