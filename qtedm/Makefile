EPICS_SEARCH_PATHS := /usr/local/oag/base ../../epics-base ../../../../base \
                      $(HOME)/epics/base $(HOME)/epics/base-7.0 \
                      /cygdrive/*/epics/base /cygdrive/*/epics/base-7.0
EPICS_BASE ?= $(firstword $(wildcard $(EPICS_SEARCH_PATHS)))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the medm repository)
endif

EPICS_BASE := $(abspath $(EPICS_BASE))

include ../Makefile.rules

# Detect if libCom.a was built with readline support by checking for an
# unresolved readline symbol in the EPICS archive for the active platform.
LIBCOM_PATH := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a
ifneq ($(OS), Windows)
  LIBCOM_NEEDS_READLINE := $(shell test -f "$(LIBCOM_PATH)" && \
    nm "$(LIBCOM_PATH)" 2>/dev/null | grep -Eq "U _?readline$$" && echo 1)
endif

PROD_LIBS :=

ifeq ($(OS), Windows)
  QT_VERSION ?= 6
  ifeq ($(QT_VERSION),5)
    QT_DIR ?= C:\\Qt\\5.15.2\\msvc2019_64
  else ifeq ($(QT_VERSION),6)
    QT_DIR ?= C:\\Qt\\6.8.2\\msvc2022_64
  else
    $(error Unsupported QT_VERSION=$(QT_VERSION). Use QT_VERSION=5 or QT_VERSION=6.)
  endif
  QT_CORE_PKG := Qt$(QT_VERSION)Core
  QT_TEST_PKG := Qt$(QT_VERSION)Test
  QT_DIR_FWD := $(subst \\,/,$(QT_DIR))
  QT_WIDGETS_CFLAGS := -I"$(QT_DIR_FWD)/include" -I"$(QT_DIR_FWD)/include/QtCore" -I"$(QT_DIR_FWD)/include/QtGui" -I"$(QT_DIR_FWD)/include/QtWidgets" -I"$(QT_DIR_FWD)/include/QtPrintSupport" -I"$(QT_DIR_FWD)/include/QtSvg"
  QT_TEST_CFLAGS := -I"$(QT_DIR_FWD)/include/QtTest"
else
  # Detect Qt version: prefer Qt6, fallback to Qt5
  QT6_AVAILABLE := $(shell pkg-config --exists Qt6Core 2>/dev/null && echo 1)
  ifeq ($(QT6_AVAILABLE),1)
    QT_VERSION := 6
    QT_WIDGETS_PKG := Qt6Widgets
    QT_PRINT_PKG := Qt6PrintSupport
    QT_SVG_PKG := Qt6Svg
    QT_CORE_PKG := Qt6Core
    QT_TEST_PKG := Qt6Test
  else
    QT5_AVAILABLE := $(shell pkg-config --exists Qt5Core 2>/dev/null && echo 1)
    ifeq ($(QT5_AVAILABLE),1)
      QT_VERSION := 5
      QT_WIDGETS_PKG := Qt5Widgets
      QT_PRINT_PKG := Qt5PrintSupport
      QT_SVG_PKG := Qt5Svg
      QT_CORE_PKG := Qt5Core
      QT_TEST_PKG := Qt5Test
    else
      $(error Neither Qt6 nor Qt5 found. Please install Qt development packages.)
    endif
  endif
  QT_WIDGETS_CFLAGS := $(shell pkg-config --cflags $(QT_WIDGETS_PKG))
endif

ifeq ($(OS), Windows)
  QT_RCC := "$(QT_DIR_FWD)/bin/rcc.exe"
  RCC_OBJ_OUT := /Fo
else
  QT_RCC_DIR := $(shell pkg-config --variable=host_bins $(QT_CORE_PKG) 2>/dev/null)
  ifeq ($(QT_RCC_DIR),)
    ifeq ($(OS), Darwin)
      QT_RCC := $(firstword $(wildcard \
              /opt/homebrew/bin/rcc \
              /opt/homebrew/opt/qt@6/bin/rcc \
              /opt/homebrew/opt/qt/bin/rcc \
              /opt/local/libexec/qt6/bin/rcc \
              /usr/local/opt/qt@6/bin/rcc \
              /usr/local/opt/qt/bin/rcc \
              /usr/bin/rcc))
      ifeq ($(strip $(QT_RCC)),)
        QT_RCC := rcc
      endif
    else
      QT_RCC := rcc
    endif
  else
    QT_RCC := $(QT_RCC_DIR)/rcc
  endif
  RCC_OBJ_OUT := -o 
endif

ifneq ($(OS), Windows)
  PROD_LIBS += -lANLwidgets -lPrintUtils
endif

PROD = qtedm

qtedm_SRC = qtedm.cc \
            command_line_options.cc \
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
            text_area_element.cc \
            text_area_runtime.cc \
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
            led_monitor_element.cc \
            led_monitor_runtime.cc \
            expression_channel_element.cc \
            expression_channel_runtime.cc \
            bar_monitor_element.cc \
            bar_monitor_runtime.cc \
            thermometer_element.cc \
            thermometer_runtime.cc \
            channel_access_context.cc \
            shared_channel_manager.cc \
            soft_pv_registry.cc \
            pvaSDDS.cc \
            pva_bridge.cc \
            pva_channel_manager.cc \
            pva_info_snapshot.cc \
            pv_channel_manager.cc \
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
            heatmap_element.cc \
            heatmap_runtime.cc \
            waterfall_plot_element.cc \
            waterfall_plot_runtime.cc \
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
            memory_tracker.cc \
            cursor_utils.cc \
            text_runtime_calc_bridge.c \
            update_coordinator.cc

qtedm_OBJS = $(patsubst %.cc,$(OBJ_DIR)/%.$(OBJEXT),$(qtedm_SRC))

CFLAGS += -I. -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc
CCFLAGS += -I. -I$(EPICS_BASE)/include -I$(OBJ_DIR) -I../printUtils -I../xc
ifeq ($(OS), Windows)
 CCFLAGS += /std:c++20 /Zc:__cplusplus /permissive- $(QT_WIDGETS_CFLAGS)
 LDFLAGS += /LIBPATH:"$(QT_DIR_FWD)/lib" Qt$(QT_VERSION)PrintSupport.lib Qt$(QT_VERSION)Svg.lib Qt$(QT_VERSION)Widgets.lib Qt$(QT_VERSION)Gui.lib Qt$(QT_VERSION)Core.lib
else
 CCFLAGS += $(shell pkg-config --cflags $(QT_WIDGETS_PKG) $(QT_PRINT_PKG) $(QT_SVG_PKG))
 LDFLAGS += $(shell pkg-config --libs $(QT_WIDGETS_PKG) $(QT_PRINT_PKG) $(QT_SVG_PKG))
endif

ifeq ($(OS), Windows)
  QT_TEST_CFLAGS :=
  QT_TEST_LIBS := Qt$(QT_VERSION)Test.lib
  QT_TEST_AVAILABLE := 1
else
  QT_TEST_AVAILABLE := $(shell pkg-config --exists $(QT_TEST_PKG) 2>/dev/null && echo 1)
  QT_TEST_CFLAGS := $(shell pkg-config --cflags $(QT_TEST_PKG))
  QT_TEST_LIBS := $(shell pkg-config --libs $(QT_TEST_PKG))
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
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
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
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) \
    $(Z_LIB) $(if $(LIBCOM_NEEDS_READLINE),-lreadline) $(PROD_SYS_LIBS)
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
                /opt/homebrew/bin/moc \
                /opt/homebrew/opt/qt@6/libexec/moc \
                /opt/homebrew/opt/qt@6/bin/moc \
                /opt/homebrew/opt/qt/libexec/moc \
                /opt/homebrew/opt/qt/bin/moc \
                /opt/local/libexec/qt6/bin/moc \
                /usr/local/opt/qt@6/bin/moc \
                /usr/local/opt/qt/bin/moc))
      else
        MOC = $(firstword $(wildcard \
                /opt/homebrew/bin/moc \
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
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL -DNOMINMAX
  CCFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL -DNOMINMAX
  PROD_LIBS += gsl.lib gslcblas.lib ws2_32.lib \
               pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib
  LIB_LINK_DIRS += -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
  MOC = "$(QT_DIR_FWD)/bin/moc.exe"

  PVA_CXX17_OBJS = $(OBJ_DIR)/pvaSDDS.$(OBJEXT) \
                   $(OBJ_DIR)/pva_bridge.$(OBJEXT)

$(PVA_CXX17_OBJS): CCFLAGS := $(filter-out /std:c++20 /permissive-,$(CCFLAGS)) /std:c++17
endif

include ../Makefile.build

qtedm_OBJS += $(OBJ_DIR)/icons_rcc.$(OBJEXT)
qtedm_OBJS += $(OBJ_DIR)/help_rcc.$(OBJEXT)
qtedm_OBJS += $(OBJ_DIR)/demo_rcc.$(OBJEXT)

$(OBJ_DIR)/icons_rcc.$(OBJEXT): resources/icons.qrc resources/icons/QtEDM.png | $(OBJ_DIR)
	$(QT_RCC) -name icons resources/icons.qrc -o $(OBJ_DIR)/icons_rcc.cpp
	$(CCC) $(CCFLAGS) -c $(OBJ_DIR)/icons_rcc.cpp $(RCC_OBJ_OUT)$@

$(OBJ_DIR)/help_rcc.$(OBJEXT): resources/help.qrc ../docs/QtEDM.html \
                               $(wildcard ../docs/images/qtedm-widgets/*.png) \
                               | $(OBJ_DIR)
	$(QT_RCC) -name help resources/help.qrc -o $(OBJ_DIR)/help_rcc.cpp
	$(CCC) $(CCFLAGS) -c $(OBJ_DIR)/help_rcc.cpp $(RCC_OBJ_OUT)$@

$(OBJ_DIR)/demo_rcc.$(OBJEXT): resources/demo.qrc \
                               resources/demo/QtEDM_Demo.adl | $(OBJ_DIR)
	$(QT_RCC) -name demo resources/demo.qrc -o $(OBJ_DIR)/demo_rcc.cpp
	$(CCC) $(CCFLAGS) -c $(OBJ_DIR)/demo_rcc.cpp $(RCC_OBJ_OUT)$@

$(OBJ_DIR)/moc_cartesian_plot_element.cpp: cartesian_plot_element.h | $(OBJ_DIR)
	$(MOC) $(QT_WIDGETS_CFLAGS) $< -o $@

$(OBJ_DIR)/cartesian_plot_element.$(OBJEXT): $(OBJ_DIR)/moc_cartesian_plot_element.cpp

$(OBJ_DIR)/moc_find_pv_dialog.cpp: find_pv_dialog.h | $(OBJ_DIR)
	$(MOC) $(QT_WIDGETS_CFLAGS) $< -o $@

$(OBJ_DIR)/find_pv_dialog.$(OBJEXT): $(OBJ_DIR)/moc_find_pv_dialog.cpp

$(OBJ_DIR)/moc_shared_channel_manager.cpp: shared_channel_manager.h | $(OBJ_DIR)
	$(MOC) $(QT_WIDGETS_CFLAGS) $< -o $@

$(OBJ_DIR)/shared_channel_manager.$(OBJEXT): $(OBJ_DIR)/moc_shared_channel_manager.cpp

$(OBJ_DIR)/moc_memory_tracker.cpp: memory_tracker.h | $(OBJ_DIR)
	$(MOC) $(QT_WIDGETS_CFLAGS) $< -o $@

$(OBJ_DIR)/memory_tracker.$(OBJEXT): $(OBJ_DIR)/moc_memory_tracker.cpp

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

QTEDM_UNIT_TEST_NAMES = test_command_line \
                        test_adl_parser \
                        test_adl_writer \
                        test_text_format_utils \
                        test_runtime_utils

QTEDM_VISUAL_TEST_TOOLS = $(OBJ_DIR)/qtedm_image_compare$(EXEEXT)

QTEDM_UNIT_TEST_BINARIES = $(addprefix $(OBJ_DIR)/,$(addsuffix $(EXEEXT),$(QTEDM_UNIT_TEST_NAMES)))
QTEDM_UNIT_TEST_MOCS = $(addprefix $(OBJ_DIR)/,$(addsuffix .moc,$(QTEDM_UNIT_TEST_NAMES)))

test_command_line_OBJS = $(OBJ_DIR)/test_command_line.$(OBJEXT) \
                         $(OBJ_DIR)/command_line_options.$(OBJEXT)
test_adl_parser_OBJS = $(OBJ_DIR)/test_adl_parser.$(OBJEXT) \
                       $(OBJ_DIR)/adl_parser.$(OBJEXT)
test_adl_writer_OBJS = $(OBJ_DIR)/test_adl_writer.$(OBJEXT) \
                       $(OBJ_DIR)/adl_writer.$(OBJEXT) \
                       $(OBJ_DIR)/medm_colors.$(OBJEXT)
test_text_format_utils_OBJS = $(OBJ_DIR)/test_text_format_utils.$(OBJEXT) \
                              $(OBJ_DIR)/text_format_utils.$(OBJEXT)
test_runtime_utils_OBJS = $(OBJ_DIR)/test_runtime_utils.$(OBJEXT) \
                          $(OBJ_DIR)/runtime_utils.$(OBJEXT)
qtedm_image_compare_OBJS = $(OBJ_DIR)/qtedm_image_compare.$(OBJEXT)

$(OBJ_DIR)/test_%.moc: tests/test_%.cc | $(OBJ_DIR)
	$(MOC) $(QT_WIDGETS_CFLAGS) $(QT_TEST_CFLAGS) $< -o $@

$(OBJ_DIR)/test_%.$(OBJEXT): tests/test_%.cc $(OBJ_DIR)/test_%.moc | $(OBJ_DIR)
	$(CCC) $(CCFLAGS) $(QT_TEST_CFLAGS) $(DEPFLAGS) -c $< $(OUTPUT)

$(OBJ_DIR)/qtedm_image_compare.$(OBJEXT): tests/qtedm_image_compare.cc | $(OBJ_DIR)
	$(CCC) $(CCFLAGS) $(DEPFLAGS) -c $< $(OUTPUT)

define link_qtedm_unit_test
$(OBJ_DIR)/$(1)$(EXEEXT): $$($(1)_OBJS)
	$$(LINKEXE) $$(OUTPUTEXE) $$^ $$(LDFLAGS) $$(QT_TEST_LIBS) $$(PROD_SYS_LIBS)
endef

$(foreach test,$(QTEDM_UNIT_TEST_NAMES),$(eval $(call link_qtedm_unit_test,$(test))))

$(OBJ_DIR)/qtedm_image_compare$(EXEEXT): $(qtedm_image_compare_OBJS)
	$(LINKEXE) $(OUTPUTEXE) $^ $(LDFLAGS)

check-qt-test:
ifeq ($(QT_TEST_AVAILABLE),1)
	@:
else
	@echo "ERROR: $(QT_TEST_PKG) development package not found."
	@exit 1
endif

tests: check-qt-test $(QTEDM_UNIT_TEST_BINARIES)

test: test-cli test-unit test-ioc test-visual

test-unit: tests
	@set -e; \
	tmp_dir=$$(mktemp -d "$${TMPDIR:-/tmp}/qtedm-unit.XXXXXX"); \
	trap 'rm -rf "$$tmp_dir"' EXIT HUP INT TERM; \
	export QT_QPA_PLATFORM=offscreen; \
	export QTEDM_NOLOG=1; \
	export LC_ALL=C; \
	export TZ=UTC; \
	export HOME="$$tmp_dir/home"; \
	mkdir -p "$$HOME"; \
	for test_bin in $(QTEDM_UNIT_TEST_BINARIES); do \
	  echo "Running $$test_bin"; \
	  "$$test_bin"; \
	done

test-cli: $(OBJ_DIR)/qtedm$(EXEEXT)
	QTEDM_BIN="$(CURDIR)/$(OBJ_DIR)/qtedm$(EXEEXT)" ../tests/run_qtedm_cli_tests.sh

test-ioc: $(OBJ_DIR)/qtedm$(EXEEXT)
	QTEDM_BIN="$(CURDIR)/$(OBJ_DIR)/qtedm$(EXEEXT)" ../tests/run_qtedm_ioc_tests.sh

test-visual: $(OBJ_DIR)/qtedm$(EXEEXT) $(QTEDM_VISUAL_TEST_TOOLS)
	QTEDM_BIN="$(CURDIR)/$(OBJ_DIR)/qtedm$(EXEEXT)" \
	QTEDM_IMAGE_COMPARE_BIN="$(CURDIR)/$(OBJ_DIR)/qtedm_image_compare$(EXEEXT)" \
	../tests/run_qtedm_visual_tests.sh

clean-tests:
	rm -f $(QTEDM_UNIT_TEST_BINARIES) $(QTEDM_UNIT_TEST_MOCS) \
	  $(QTEDM_VISUAL_TEST_TOOLS)

.PHONY: tests test test-unit test-cli test-ioc test-visual clean-tests check-qt-test

.NOTPARALLEL: test
