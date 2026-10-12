
	STRIP ?= strip

	# check on which OS we build
	UNAME_S := $(shell uname -s)
	# check which architecture we build on
	UNAME_M := $(shell uname -m)
	# check which architecture we build for
	TARGET_ARCH = $(shell $(CXX) -dumpmachine | sed 's/-.*//')

ifneq ($(UNAME_M),$(TARGET_ARCH))
  CROSS_COMPILING = 1
  $(info $(yellow) INFO: $(reset)Cross Compile $(blue)$(UNAME_M)$(reset) to $(blue)$(TARGET_ARCH)$(reset))
endif


# avoid optimisation for x86_64 arch when we cross compile
ifneq ($(CROSS_COMPILING), 1)

# get flags supported by CPU
ifeq ($(UNAME_S), FreeBSD)
  CPU_FLAGS := $(shell dmesg | grep Features | tr 'A-Z' 'a-z' | tr '<>,=' '    ')
  ifeq ($(UNAME_M), amd64)
    UNAME_M = x86_64
  endif
else
  CPU_FLAGS := $(shell grep -m1 -i '^flags' /proc/cpuinfo 2>/dev/null)
endif

ifeq ($(TARGET_ARCH), x86_64)
  ifneq ($(filter pni sse3,$(CPU_FLAGS)),)
    SSE_CFLAGS = -msse3 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG   = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($(filter sse2,$(CPU_FLAGS)),)
    SSE_CFLAGS = -msse2 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG   = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($(filter sse,$(CPU_FLAGS)),)
    SSE_CFLAGS = -msse -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG   = -DFFTCONVOLVER_USE_SSE=1
  endif
else ifeq ($(TARGET_ARCH), arm)
  ARM_FEATURES := $(shell grep -m1 -i '^features' /proc/cpuinfo 2>/dev/null)
  ifneq ($(filter vfpv3 vfpd32,$(ARM_FEATURES)),)
    SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3
  endif
else ifeq ($(TARGET_ARCH), aarch64)
  ARMCPU = "YES"
endif

endif



ifeq ($(MAKECMDGOALS),debug)
  CXXFLAGS += -g -DDEBUG
else
  CXXFLAGS += -O3 -DNDEBUG
endif

	# set bundle name
	NAME = toneshifteq
	VST3_NAME = Toneshifteq
	VER = 2.0.0

	PREFIX ?= /usr
	# check if user is root
	user = $(shell whoami)
	ifeq ($(user),root)
	LV2_INSTALL_DIR ?= $(PREFIX)/lib/lv2
	EXE_INSTALL_DIR ?= $(PREFIX)/bin
	CLAP_INSTALL_DIR ?= $(PREFIX)/lib/clap
	VST3_INSTALL_DIR ?= $(PREFIX)/lib/vst3
	else 
	LV2_INSTALL_DIR ?= ~/.lv2
	EXE_INSTALL_DIR ?= ~/bin
	CLAP_INSTALL_DIR ?= ~/.clap
	VST3_INSTALL_DIR ?= ~/.vst3
	endif

	SHARE_DIR ?= $(PREFIX)/share
	DESKAPPS_DIR ?= $(SHARE_DIR)/applications
	PIXMAPS_DIR ?= $(SHARE_DIR)/pixmaps
	PKGCONFIG = pkg-config

	INCLUDES := -I.. -I./Convolver -I./Engine -I./FileIO -I./GUI -I./IRMatch -I./AudioFFT
	LIB_DIR := ../libxputty/libxputty/
	HEADER_DIR := $(LIB_DIR)include/
	UI_LIB:= $(LIB_DIR)libxputty.$(STATIC_LIB_EXT)

	FFT_DIR := ./AudioFFT/
	FFT_SOURCE := $(FFT_DIR)AudioFFT.cpp
	FFT_OBJ    := AudioFFT.o

	BUNDLE := ToneShiftEQ.lv2
	LV2_DIR := ./lv2/
	LV2_INCLUDE := -I./lv2/

	LV2_PLUGIN := $(LV2_DIR)ToneShiftEQ
	LV2_GUI := $(LV2_DIR)ToneShiftEQ_ui

	CLAP_DIR := ./clap/
	CLAP_INCLUDE := -I./clap/clap/ -I./clap/
	CLAP_SOURCES := $(CLAP_DIR)ToneShiftEQClap.cpp $(FFT_OBJ)

	VST3_DIR := ./vst3/
	VST3_INCLUDE := -I./vst3/travesty -I./vst3/
	VST3_SOURCES := $(VST3_DIR)ToneShiftEQVST3.cpp $(FFT_OBJ)
	ARCH = `echo $(UNAME_S) | tr A-Z a-z`
	VST3_BUNDLE := $(VST3_NAME).vst3/Contents/$(UNAME_M)-$(ARCH)/

ifeq ($(TARGET), Linux)
	# set compile flags
	GUI_LDFLAGS += -I$(HEADER_DIR) -Wl,-Bstatic -L. $(UI_LIB) \
	-fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--exclude-libs,ALL \
	-Wl,-Bdynamic `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags --libs cairo x11`

	CFLAGS += -Wall -funroll-loops `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags sndfile `\
	-ffast-math -ffinite-math-only -fomit-frame-pointer -fstrength-reduce -fdata-sections \
	-pthread $(SSE_CFLAGS)

	S_CFLAGS = `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags jack `

	CXXFLAGS += -MMD -std=c++17 -fno-math-errno -fno-trapping-math -fPIC -D_OS_UNIX_ -DALVER=\"$(VER)\" $(CFLAGS) \
	-fvisibility=hidden -fvisibility-inlines-hidden

	LDFLAGS += $(INCLUDES) -lm -pthread -lpthread -lstdc++ $(GUI_LDFLAGS) \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --libs sndfile $(BACKEND) `
	ifneq ($(MACOS)$(WINDOWS),true)
		LDFLAGS += -lrt -lc
	endif

	S_LDFLAGS = `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --libs jack `

	ifneq (,$(findstring clang, $(CXX)))
		CXXFLAGS := $(filter-out -fstrength-reduce,$(CXXFLAGS))
		CXXFLAGS += -Wno-unused-private-field
	endif
else ifeq ($(TARGET), Windows)
	VST3_BUNDLE := $(VST3_NAME).vst3/Contents/$(UNAME_M)-win/

	LDFLAGS += $(INCLUDES) -I. -shared -lm $(PAWPAW_LFLAGS) -Wl,--gc-sections \
	-Wl,--exclude-libs,ALL `$(PKGCONFIG) --libs --libs sndfile `

	GUI_LDFLAGS += -I$(HEADER_DIR) -static-libgcc -static-libstdc++ \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags --libs cairo ` \
	-L. $(UI_LIB) -lm $(PAWPAW_LFLAGS)

	CFLAGS := -Wall -fstack-protector -fvisibility=hidden \
	-fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--exclude-libs,ALL \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags sndfile `

	CXXFLAGS += -std=c++17 -fno-math-errno -fno-trapping-math -fPIC -DALVER=\"$(VER)\" $(CFLAGS)

	TTLUPDATE = sed -i '/lv2:binary/ s/\.so/\.dll/ ' ../bin/$(BUNDLE)/manifest.ttl
	TTLUPDATEGUI = sed -i '/a guiext:X11UI/ s/X11UI/WindowsUI/ ; /guiext:binary/ s/\.so/\.dll/ ' ../bin/$(BUNDLE)/$(NAME).ttl

endif

	# invoke build files
	OBJECTS = main.cpp $(FFT_OBJ)

	DEPS = $(NAME)_app.d $(NAME)_lv2.d $(NAME)_clap.d $(NAME).d $(NAME)_ui.d $(NAME)_vst3.d

.PHONY : all clap lv2 standalone vst3


all : check standalone clap lv2 vst3

standalone: $(NAME)
ifneq ($(TARGET), Windows)
	$(QUIET)mkdir -p ../bin
	$(QUIET)cp ./$(NAME)$(EXE) ../bin
	@if ! [ -f ../bin/$(NAME)$(EXE) ]; then \
		$(R_ECHO) "Sorry, build fail$(reset)"; \
	fi
	@$(B_ECHO) "============== Standalone DONE =================$(reset)"
endif

clap: $(NAME).clap
	$(QUIET)mkdir -p ../bin/
	$(QUIET)cp ./$(NAME).clap ../bin/
	@$(B_ECHO) "================ Clap DONE =====================$(reset)"

lv2: $(NAME).$(LIB_EXT) $(NAME)_ui.$(LIB_EXT)
ifneq ($(TARGET), Windows)
	@$(B_ECHO) "Create $(BUNDLE) $(reset)"
	$(QUIET)mkdir -p ../bin/$(BUNDLE)
	$(QUIET)cp $(LV2_DIR)*.ttl ../bin/$(BUNDLE)
	$(QUIET)$(TTLUPDATE)
	$(QUIET)$(TTLUPDATEGUI)
	$(QUIET)cp ./$(NAME).$(LIB_EXT) ../bin/$(BUNDLE)
	$(QUIET)cp ./$(NAME)_ui.$(LIB_EXT) ../bin/$(BUNDLE)
	@if ! [ -f ../bin/$(BUNDLE)/$(NAME).$(LIB_EXT) ]; then \
		$(R_ECHO) "Sorry, build fail$(reset)"; \
	fi
	@$(B_ECHO) "================= LV2 DONE =====================$(reset)"
endif

vst3: $(VST3_NAME).$(LIB_EXT)
	@$(B_ECHO) "Create $(VST3_BUNDLE) $(reset)"
	$(QUIET)mkdir -p ../bin/
	$(QUIET)mkdir -p ../bin/$(VST3_BUNDLE)
ifeq ($(TARGET), Windows)
	$(QUIET)cp ./$(VST3_NAME).$(LIB_EXT) ../bin/$(VST3_BUNDLE)/$(VST3_NAME).vst3
else
	$(QUIET)cp ./$(VST3_NAME).$(LIB_EXT) ../bin/$(VST3_BUNDLE)
endif
	@$(B_ECHO) "================ VST3 DONE =====================$(reset)"

debug : all


-include $(DEPS)

check :
ifdef ARMCPU
	@$(R_ECHO)"ARM CPU DEDECTED, please check the optimization flags$(reset)"
endif

info:
	$(info $(yellow) INFO: $(reset) cxxflags $(blue) $(CXX) $(CXXFLAGS) $(reset))
	$(info $(yellow) INFO: $(reset) ldflags  $(blue) $(LD) $(LDFLAGS) $(GUI_LDFLAGS))


clean :
	$(QUIET)rm -f *.o *.d *.a *.lib *.clap *.so *.dll
	$(QUIET)rm -f $(NAME) $(NAME).exe 
	$(QUIET)rm -rf ../bin

dist-clean :

install :
ifneq ("$(wildcard ../bin/$(NAME)$(EXE))","")
	$(QUIET)mkdir -p $(DESTDIR)$(EXE_INSTALL_DIR)
	$(QUIET)cp ./$(BUILD_DIR)/$(NAME) $(DESTDIR)$(EXE_INSTALL_DIR)/$(NAME)
  ifeq ($(user),root)
	$(QUIET)mkdir -p $(DESTDIR)$(DESKAPPS_DIR)
	$(QUIET)cp ./Resources/$(NAME).desktop $(DESTDIR)$(DESKAPPS_DIR)/$(NAME).desktop
	$(QUIET)mkdir -p $(DESTDIR)$(PIXMAPS_DIR)
	$(QUIET)cp ./Resources/$(NAME).svg $(DESTDIR)$(PIXMAPS_DIR)/$(NAME).svg
	$(QUIET)update-desktop-database || true
  endif
else
	@$(B_ECHO) "$(NAME)$(EXE) Standalone skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(NAME).clap)","")
	@$(B_ECHO) "Install  $(NAME).clap to $(DESTDIR)$(CLAP_INSTALL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(CLAP_INSTALL_DIR)/
	$(QUIET)cp -r ../bin/$(NAME).clap $(DESTDIR)$(CLAP_INSTALL_DIR)/$(NAME).clap
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(NAME).clap Clap skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(BUNDLE))","")
	@$(B_ECHO) "Install $(NAME).lv2 to $(DESTDIR)$(LV2_INSTALL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(LV2_INSTALL_DIR)/$(BUNDLE)
	$(QUIET)cp -r ../bin/$(BUNDLE)/* $(DESTDIR)$(LV2_INSTALL_DIR)/$(BUNDLE)
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(BUNDLE) skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(VST3_NAME).vst3)","")
	@$(B_ECHO) "Install $(VST3_NAME).vst3 to $(DESTDIR)$(VST3_INSTALL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(VST3_INSTALL_DIR)/$(VST3_NAME).vst3
	$(QUIET)cp -r ../bin/$(VST3_NAME).vst3/* $(DESTDIR)$(VST3_INSTALL_DIR)/$(VST3_NAME).vst3
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(VST3_NAME).vst3 skipped$(reset)"
endif

uninstall :
	$(QUIET)rm -rf $(DESTDIR)$(EXE_INSTALL_DIR)/$(NAME)
	$(QUIET)rm -rf $(DESTDIR)$(LV2_INSTALL_DIR)/$(BUNDLE)
	$(QUIET)rm -rf $(DESTDIR)$(CLAP_INSTALL_DIR)/$(NAME).clap
	$(QUIET)rm -rf $(DESTDIR)$(VST3_INSTALL_DIR)/$(VST3_NAME).vst3

$(FFT_OBJ): $(FFT_SOURCE)
	@$(B_ECHO) "Compiling $(FFT_OBJ) $(reset)"
	$(QUIET)$(CXX) -c -MMD -MF AudioFFT.d $(CXXFLAGS) $< -o $@

$(NAME) : $(OBJECTS)
ifeq ($(TARGET), Windows)
	@$(B_ECHO) "Stand-alone not supported under Windows $(reset)"
else
	@$(B_ECHO) "Compiling $(NAME) $(reset)"
	$(QUIET)$(CXX) -MMD -MF $(NAME)_app.d $(CXXFLAGS) $(S_CFLAGS) $(OBJECTS) -o $(NAME)$(EXE) $(LDFLAGS) $(S_LDFLAGS)
ifneq ($(MAKECMDGOALS),debug)
	$(QUIET)$(STRIP) -s -x -X -R .note.ABI-tag $(NAME)$(EXE)
endif
endif

$(NAME).clap: $(CLAP_SOURCES) $(CLAP_DIR)ToneShiftEQ.cc
	@$(B_ECHO) "Compiling $(NAME).clap $(reset)"
	$(QUIET)$(CXX) -shared -MMD -MF $(NAME)_clap.d $(CXXFLAGS) $(CLAP_INCLUDE) $(CLAP_SOURCES) -o $(NAME).clap  $(LDFLAGS) $(GUI_LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME).clap
endif

$(NAME)_ui.$(LIB_EXT): $(LV2_GUI).cpp $(UI_LIB)
ifneq ($(TARGET), Windows)
	@$(B_ECHO) "Compiling $(NAME)_ui.$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) -shared -MMD -MF $(NAME)_ui.d $(CXXFLAGS) -I./ $(LV2_INCLUDE) $(LV2_GUI).cpp -L. $(UI_LIB) -o $@ $(LDFLAGS) $(GUI_LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME)_ui.$(LIB_EXT)
endif
endif

$(NAME).$(LIB_EXT): $(LV2_PLUGIN).cpp $(FFT_OBJ)
ifeq ($(TARGET), Windows)
	@$(B_ECHO) "LV2 not supported under Windows $(reset)"
else
	@$(B_ECHO) "Compiling $(NAME).$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) -shared -MMD -MF $(NAME)_lv2.d $(CXXFLAGS) $(LV2_INCLUDE) $(FFT_OBJ) $(LV2_PLUGIN).cpp -o $(NAME).$(LIB_EXT) $(LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME).$(LIB_EXT)
endif
endif

$(VST3_NAME).$(LIB_EXT): $(VST3_SOURCES)
	@$(B_ECHO) "Compiling $(VST3_NAME).$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) -shared -MMD -MF $(NAME)_vst3.d $(CXXFLAGS) $(VST3_INCLUDE) $(VST3_SOURCES) -o $(VST3_NAME).$(LIB_EXT)  $(LDFLAGS) $(GUI_LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(VST3_NAME).$(LIB_EXT)
endif


doc:
	#pass
