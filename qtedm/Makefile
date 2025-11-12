SDDS_SEARCH_PATHS := ../../SDDS ../../../../epics/extensions/src/SDDS \
                     $(HOME)/SDDS $(HOME)/epics/extensions/src/SDDS \
                     /cygdrive/*/SDDS /cygdrive/*/epics/extensions/src/SDDS
SDDS_REPO ?= $(firstword $(wildcard $(SDDS_SEARCH_PATHS)))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the medm repository)
endif
EPICS_SEARCH_PATHS := /usr/local/oag/base ../../epics-base ../../../../base \
                      $(HOME)/epics/base $(HOME)/epics/base-7.0 \
                      /cygdrive/*/epics/base /cygdrive/*/epics/base-7.0
EPICS_BASE ?= $(firstword $(wildcard $(EPICS_SEARCH_PATHS)))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the medm repository)
endif

SDDS_REPO := $(abspath $(SDDS_REPO))
EPICS_BASE := $(abspath $(EPICS_BASE))

include ../Makefile.rules

PROD_LIBS :=
PROD_LIBS_SDDS :=

QT_RCC_DIR := $(shell pkg-config --variable=host_bins Qt5Core 2>/dev/null)
ifeq ($(QT_RCC_DIR),)
  QT_RCC := rcc
else
  QT_RCC := $(QT_RCC_DIR)/rcc
endif

ifneq ($(OS), Windows)
  PROD_LIBS += -lANLwidgets -lPrintUtils
  PROD_LIBS_SDDS += -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib \
                    -lmdbmth -lmdblib
endif

PROD = qtedm

qtedm_SRC = qtedm.cc \
            adl_parser.cc \
            legacy_fonts.cc \
            window_utils.cc \
            adl_writer.cc \
            main_window_controller.cc \
            text_font_utils.cc \
            text_element.cc \
            text_runtime.cc \
            text_entry_element.cc \
            text_entry_runtime.cc \
            message_button_element.cc \
            message_button_runtime.cc \
            shell_command_element.cc \
            choice_button_element.cc \
            choice_button_runtime.cc \
            menu_element.cc \
            menu_runtime.cc \
            related_display_element.cc \
            text_monitor_element.cc \
            text_monitor_runtime.cc \
            byte_monitor_element.cc \
            byte_monitor_runtime.cc \
            bar_monitor_element.cc \
            bar_monitor_runtime.cc \
            channel_access_context.cc \
            slider_element.cc \
            slider_runtime.cc \
            wheel_switch_element.cc \
            wheel_switch_runtime.cc \
            meter_element.cc \
            meter_runtime.cc \
            scale_monitor_element.cc \
            scale_monitor_runtime.cc \
            strip_chart_element.cc \
            strip_chart_runtime.cc \
            cartesian_plot_element.cc \
            cartesian_plot_runtime.cc \
            graphic_element_runtime_base.cc \
            single_channel_monitor_runtime_base.cc \
            runtime_utils.cc \
            text_format_utils.cc \
            rectangle_element.cc \
            rectangle_runtime.cc \
            oval_element.cc \
            oval_runtime.cc \
            arc_element.cc \
            arc_runtime.cc \
            line_element.cc \
            line_runtime.cc \
            polyline_element.cc \
            polyline_runtime.cc \
            polygon_element.cc \
            polygon_runtime.cc \
            image_element.cc \
            image_runtime.cc \
            composite_element.cc \
            composite_runtime.cc \
            cartesian_axis_dialog.cc \
            display_list_dialog.cc \
            object_palette_dialog.cc \
            color_palette_dialog.cc \
            pv_limits_dialog.cc \
            pv_info_dialog.cc \
            medm_colors.cc \
            statistics_tracker.cc \
            statistics_window.cc \
            cursor_utils.cc \
            text_runtime_calc_bridge.c

qtedm_OBJS = $(patsubst %.cc,$(OBJ_DIR)/%.$(OBJEXT),$(qtedm_SRC))

CFLAGS += -I$(EPICS_BASE)/include  -I$(SDDS_REPO)/include -I$(OBJ_DIR) -I../printUtils -I../xc
CCFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I$(OBJ_DIR) -I../printUtils -I../xc
ifeq ($(OS), Windows)
 CCFLAGS += /std:c++17 /Zc:__cplusplus /permissive- -I"C:\\Qt\\6.8.2\\msvc2022_64\\include" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtCore" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtGui" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtWidgets"
 LDFLAGS += /LIBPATH:"C:\\Qt\\6.8.2\\msvc2022_64\\lib" Qt6PrintSupport.lib Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib
else
 CCFLAGS += $(shell pkg-config --cflags Qt5Widgets)
 LDFLAGS += $(shell pkg-config --libs Qt5Widgets)
endif


ifeq ($(OS), Linux)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  CCFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) \
    $(Z_LIB) -lreadline $(PROD_SYS_LIBS)
  PROD_SYS_LIBS += -lX11
endif

ifeq ($(OS), Darwin)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-deprecated-builtins
  CCFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
endif

ifeq ($(OS), Windows)
  NO_DLL = 1
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -I$(SDDS_REPO)/lzma -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  CCFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -I$(SDDS_REPO)/lzma -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  PROD_LIBS += gsl.lib gslcblas.lib ws2_32.lib
  PROD_LIBS_SDDS := mdbcommon.lib matlib.lib SDDS1.lib namelist.lib rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib \
                  pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib
  LIB_LINK_DIRS += -LIBPATH:$(SDDS_REPO)/lib/$(OS)-$(ARCH) -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
endif

include ../Makefile.build

qtedm_OBJS += $(OBJ_DIR)/icons_rcc.$(OBJEXT)

$(OBJ_DIR)/icons_rcc.$(OBJEXT): resources/icons.qrc resources/icons/related_display.xpm | $(OBJ_DIR)
	$(QT_RCC) -name icons resources/icons.qrc -o $(OBJ_DIR)/icons_rcc.cpp
	$(CCC) $(CCFLAGS) -c $(OBJ_DIR)/icons_rcc.cpp -o $@

$(OBJ_DIR)/moc_cartesian_plot_element.cpp: cartesian_plot_element.h | $(OBJ_DIR)
	moc $(shell pkg-config --cflags Qt5Widgets) $< -o $@

$(OBJ_DIR)/cartesian_plot_element.$(OBJEXT): $(OBJ_DIR)/moc_cartesian_plot_element.cpp

$(OBJ_DIR)/qtedm$(EXEEXT): $(qtedm_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(qtedm_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/medm$(EXEEXT): $(medm_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(medm_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
