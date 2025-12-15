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

# Detect Qt version: prefer Qt6, fallback to Qt5
QT6_AVAILABLE := $(shell pkg-config --exists Qt6Core 2>/dev/null && echo 1)
ifeq ($(QT6_AVAILABLE),1)
  QT_VERSION := 6
  QT_WIDGETS_PKG := Qt6Widgets
  QT_PRINT_PKG := Qt6PrintSupport
  QT_SVG_PKG := Qt6Svg
  QT_CORE_PKG := Qt6Core
else
  QT5_AVAILABLE := $(shell pkg-config --exists Qt5Core 2>/dev/null && echo 1)
  ifeq ($(QT5_AVAILABLE),1)
    QT_VERSION := 5
    QT_WIDGETS_PKG := Qt5Widgets
    QT_PRINT_PKG := Qt5PrintSupport
    QT_SVG_PKG := Qt5Svg
    QT_CORE_PKG := Qt5Core
  else
    $(error Neither Qt6 nor Qt5 found. Please install Qt development packages.)
  endif
endif

QT_RCC_DIR := $(shell pkg-config --variable=host_bins $(QT_CORE_PKG) 2>/dev/null)
ifeq ($(QT_RCC_DIR),)
  QT_RCC := rcc
else
  QT_RCC := $(QT_RCC_DIR)/rcc
endif

ifneq ($(OS), Windows)
  PROD_LIBS += -lANLwidgets -lPrintUtils
endif

PROD = qtedm

qtedm_SRC = qtedm.cc \
            adl_parser.cc \
            audit_logger.cc \
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
            shared_channel_manager.cc \
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
            strip_chart_data_dialog.cc \
            cartesian_plot_element.cc \
            cartesian_plot_runtime.cc \
            graphic_element_runtime_base.cc \
            graphic_shape_element.cc \
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
            audit_log_viewer_dialog.cc \
            find_pv_dialog.cc \
            object_palette_dialog.cc \
            color_palette_dialog.cc \
            pv_limits_dialog.cc \
            pv_info_dialog.cc \
            medm_colors.cc \
            statistics_tracker.cc \
            statistics_window.cc \
            cursor_utils.cc \
            text_runtime_calc_bridge.c \
            update_coordinator.cc

qtedm_OBJS = $(patsubst %.cc,$(OBJ_DIR)/%.$(OBJEXT),$(qtedm_SRC))

CFLAGS += -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc
CCFLAGS += -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc
ifeq ($(OS), Windows)
 CCFLAGS += /std:c++17 /Zc:__cplusplus /permissive- -I"C:\\Qt\\6.8.2\\msvc2022_64\\include" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtCore" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtGui" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtWidgets" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtSvg"
 LDFLAGS += /LIBPATH:"C:\\Qt\\6.8.2\\msvc2022_64\\lib" Qt6PrintSupport.lib Qt6Svg.lib Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib
else
 CCFLAGS += $(shell pkg-config --cflags $(QT_WIDGETS_PKG) $(QT_PRINT_PKG) $(QT_SVG_PKG))
 LDFLAGS += $(shell pkg-config --libs $(QT_WIDGETS_PKG) $(QT_PRINT_PKG) $(QT_SVG_PKG))
endif


ifeq ($(OS), Linux)
  # Try to get MOC from pkg-config first, then fallback to known paths
  MOC_DIR := $(shell pkg-config --variable=libexecdir $(QT_CORE_PKG) 2>/dev/null)
  ifneq ($(MOC_DIR),)
    MOC := $(MOC_DIR)/moc
  else
    MOC_DIR := $(shell pkg-config --variable=host_bins $(QT_CORE_PKG) 2>/dev/null)
    ifneq ($(MOC_DIR),)
      MOC := $(MOC_DIR)/moc
    else
      ifeq ($(QT_VERSION),6)
        MOC = $(firstword $(wildcard \
                /usr/lib64/qt6/libexec/moc \
                /usr/lib/qt6/libexec/moc \
                /usr/lib/x86_64-linux-gnu/qt6/libexec/moc \
                /usr/bin/moc-qt6 \
                /usr/bin/moc))
      else
        MOC = $(firstword $(wildcard \
                /usr/lib64/qt5/bin/moc \
                /usr/lib/qt5/bin/moc \
                /usr/lib/x86_64-linux-gnu/qt5/bin/moc \
                /usr/bin/moc-qt5 \
                /usr/bin/moc))
      endif
    endif
  endif
  ifeq ($(strip $(MOC)),)
    $(error Qt moc tool not found. Please install Qt development packages: sudo apt-get install qt6-base-dev (Debian/Ubuntu) or sudo dnf install qt6-qtbase-devel (Fedora/RHEL))
  endif
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
  PROD_SYS_LIBS += -lX11
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
  # Try to get MOC from pkg-config first, then fallback to known paths
  MOC_DIR := $(shell pkg-config --variable=libexecdir $(QT_CORE_PKG) 2>/dev/null)
  ifneq ($(MOC_DIR),)
    MOC := $(MOC_DIR)/moc
  else
    MOC_DIR := $(shell pkg-config --variable=host_bins $(QT_CORE_PKG) 2>/dev/null)
    ifneq ($(MOC_DIR),)
      MOC := $(MOC_DIR)/moc
    else
      ifeq ($(QT_VERSION),6)
        MOC = $(firstword $(wildcard \
                /opt/homebrew/opt/qt@6/libexec/moc \
                /opt/homebrew/opt/qt/libexec/moc \
                /opt/local/libexec/qt6/bin/moc \
                /usr/local/opt/qt@6/bin/moc))
      else
        MOC = $(firstword $(wildcard \
                /opt/homebrew/opt/qt@5/bin/moc \
                /opt/local/libexec/qt5/bin/moc \
                /usr/local/opt/qt@5/bin/moc))
      endif
    endif
  endif
  ifeq ($(strip $(MOC)),)
    MOC = moc
  endif
endif

ifeq ($(OS), Windows)
  NO_DLL = 1
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  CCFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  PROD_LIBS += gsl.lib gslcblas.lib ws2_32.lib \
               pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib
  LIB_LINK_DIRS += -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
  MOC = moc.exe
endif

include ../Makefile.build

qtedm_OBJS += $(OBJ_DIR)/icons_rcc.$(OBJEXT)
qtedm_OBJS += $(OBJ_DIR)/help_rcc.$(OBJEXT)

$(OBJ_DIR)/icons_rcc.$(OBJEXT): resources/icons.qrc resources/icons/QtEDM.png | $(OBJ_DIR)
	$(QT_RCC) -name icons resources/icons.qrc -o $(OBJ_DIR)/icons_rcc.cpp
	$(CCC) $(CCFLAGS) -c $(OBJ_DIR)/icons_rcc.cpp -o $@

$(OBJ_DIR)/help_rcc.$(OBJEXT): resources/help.qrc ../docs/QtEDM.html | $(OBJ_DIR)
	$(QT_RCC) -name help resources/help.qrc -o $(OBJ_DIR)/help_rcc.cpp
	$(CCC) $(CCFLAGS) -c $(OBJ_DIR)/help_rcc.cpp -o $@

$(OBJ_DIR)/moc_cartesian_plot_element.cpp: cartesian_plot_element.h | $(OBJ_DIR)
	$(MOC) $(shell pkg-config --cflags $(QT_WIDGETS_PKG)) $< -o $@

$(OBJ_DIR)/cartesian_plot_element.$(OBJEXT): $(OBJ_DIR)/moc_cartesian_plot_element.cpp

$(OBJ_DIR)/moc_find_pv_dialog.cpp: find_pv_dialog.h | $(OBJ_DIR)
	$(MOC) $(shell pkg-config --cflags $(QT_WIDGETS_PKG)) $< -o $@

$(OBJ_DIR)/find_pv_dialog.$(OBJEXT): $(OBJ_DIR)/moc_find_pv_dialog.cpp

$(OBJ_DIR)/moc_shared_channel_manager.cpp: shared_channel_manager.h | $(OBJ_DIR)
	$(MOC) $(shell pkg-config --cflags $(QT_WIDGETS_PKG)) $< -o $@

$(OBJ_DIR)/shared_channel_manager.$(OBJEXT): $(OBJ_DIR)/moc_shared_channel_manager.cpp

$(OBJ_DIR)/qtedm$(EXEEXT): $(qtedm_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(qtedm_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/medm$(EXEEXT): $(medm_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(medm_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
