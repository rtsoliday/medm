EPICS_REQUIRED_LIBS := ca Com
# Retain the compiler-default C++ mode for legacy Windows builds.
WINDOWS_CXX_STD_FLAGS :=
include ../Makefile.rules

PROD_LIBS :=

ifneq ($(OS), Windows)
  PROD_LIBS += -lANLwidgets -lPrintUtils
endif

PROD = medm

medm_SRC = medmCartesianPlot.c medmSciPlot.c SciPlot.c medmCA.c actions.c bubbleHelp.c callbacks.c browserHelp.c channelPalette.c colorPalette.c dialogs.c display.c eventHandlers.c help.c medm.c medmArc.c medmBar.c medmByte.c medmChoiceButtons.c medmCommon.c medmComposite.c medmControl.c medmDisplay.c medmImage.c medmIndicator.c medmMenu.c medmMessageButton.c medmMeter.c medmMonitor.c medmOval.c medmPixmap.c medmPolygon.c medmPolyline.c medmRectangle.c medmRelatedDisplay.c medmShellCommand.c medmStripChart.c medmText.c medmTextEntry.c medmTextUpdate.c medmValuator.c medmWheelSwitch.c medmWidget.c objectPalette.c productDescriptionShell.c resourcePalette.c updateMonitors.c updateTask.c utils.c xgif.c

CFLAGS += -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc -DSCIPLOT -DMOTIF -DEDITRES
override CXXFLAGS += -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc -DSCIPLOT -DMOTIF -DEDITRES
CFLAGS += -I$(MOTIF_INC) -I$(X11_INC)
override CXXFLAGS += -I$(MOTIF_INC) -I$(X11_INC)

ifeq ($(OS), Linux)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  override CXXFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_LIBS) \
    $(LZMA_LIB) $(GSL_LINK_LIBS) \
    $(Z_LIB) $(EPICS_READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS += $(XM_LIB) $(MOTIF_STATIC_DEP_LIBS) $(XMU_LIB) $(XT_LIB) \
    $(X11_LIB) $(XEXT_LIB)
endif

ifeq ($(OS), Darwin)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-deprecated-builtins
  override CXXFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_LIBS) \
    $(LZMA_LIB) $(GSL_LINK_LIBS) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS += $(XM_LIB) $(MOTIF_STATIC_DEP_LIBS) $(XMU_LIB) $(XT_LIB) \
    $(X11_LIB) $(XEXT_LIB)
  PROD_SYS_LIBS += $(XFT_LIB) $(FONTCONFIG_LIB) $(SM_LIB) $(ICE_LIB) $(XP_LIB) $(XAU_LIB) $(JPEG_LIB) $(PNG_LIB) $(ICONV_LIB)
endif

ifeq ($(OS), Windows)
  NO_DLL = 1
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  override CXXFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  PROD_LIBS += ANLwidgets.lib PrintUtils.lib $(GSL_LINK_LIBS)
  PROD_SYS_LIBS += $(EPICS_LIBS) ws2_32.lib
  LIB_LINK_DIRS += -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
endif
CUSTOM_LINK_PRODUCTS += medm

include ../Makefile.build

$(OBJ_DIR)/medm$(EXEEXT): $(medm_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(medm_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
