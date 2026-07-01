
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
  CPU_INFO = dmesg | grep Features | tr A-Z a-z
  ifeq ($(UNAME_M), amd64)
    UNAME_M = x86_64
  endif
else
  CPU_INFO = cat /proc/cpuinfo | grep flags
endif

# check for sse optimisation level only on x86_64 architecture
ifeq ($(TARGET_ARCH), x86_64)
  ifneq ($$(filter $(CPU_INFO) | grep sse3 ) , )
    SSE_CFLAGS = -msse3 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep sse2 ) , )
    SSE_CFLAGS = -msse2 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep sse ) , )
    SSE_CFLAGS = -msse -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep ARM ) , )
    ifneq ($$(filter $(CPU_INFO) | grep ARMv7 ) , )
      ifneq ($$(filter $(CPU_INFO) | grep vfpd32 ) , )
        SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3 
      else ifneq ($$(filter $(CPU_INFO) | grep vfpv3 ) , )
        SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3
      endif
    else
      ARMCPU = "YES"
    endif
  endif
else
  SSE_CFLAGS =
endif

endif


ifeq ($(MAKECMDGOALS),debug)
  CXXFLAGS += -g -DDEBUG
else
  CXXFLAGS += -O3 -DNDEBUG
endif

	# set bundle name
	NAME = toneshifteq
	PLUGNAME = toneshifteq
	VER = 0.1

ifeq (,$(filter clap,$(MAKECMDGOALS)))
ifeq (,$(filter lv2,$(MAKECMDGOALS)))
	BACKEND = jack
endif
endif

	PREFIX ?= /usr
	# check if user is root
	user = $(shell whoami)
	ifeq ($(user),root)
	LV2_INSTALL_DIR ?= $(PREFIX)/lib/lv2
	EXE_INSTALL_DIR ?= $(PREFIX)/bin
	CLAP_INSTAL_DIR ?= $(PREFIX)/lib/clap
	else 
	LV2_INSTALL_DIR ?= ~/.lv2
	EXE_INSTALL_DIR ?= ~/bin
	CLAP_INSTAL_DIR ?= ~/.clap
	endif

	SHARE_DIR ?= $(PREFIX)/share
	DESKAPPS_DIR ?= $(SHARE_DIR)/applications
	PIXMAPS_DIR ?= $(SHARE_DIR)/pixmaps
	PKGCONFIG = pkg-config

	INCLUDES := -I.. -I./Convolver -I./Engine -I./FileIO -I./GUI -I./IRMatch
	LIB_DIR := ../libxputty/libxputty/
	HEADER_DIR := $(LIB_DIR)include/
	UI_LIB:= $(LIB_DIR)libxputty.$(STATIC_LIB_EXT)

	BUNDLE := ToneShiftEQ.lv2
	LV2_DIR := ./lv2/
	LV2_INCLUDE := -I./lv2/

	LV2_PLUGIN := $(LV2_DIR)ToneShiftEQ
	LV2_GUI := $(LV2_DIR)ToneShiftEQ_ui

	CLAP_DIR := ./clap/
	CLAP_INCLUDE := -I./clap/clap/ -I./clap/
	CLAP_SOURCES := $(CLAP_DIR)ToneShiftEQClap.cpp

	# set compile flags
	GUI_LDFLAGS += -I$(HEADER_DIR) -Wl,-Bstatic -L. $(UI_LIB) \
	-Wl,-Bdynamic `pkg-config --cflags --libs cairo x11` 

	CFLAGS += -Wall -funroll-loops `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags sndfile fftw3 $(BACKEND) `\
	-ffast-math -fomit-frame-pointer -fstrength-reduce -fdata-sections \
	-pthread $(SSE_CFLAGS)

	CXXFLAGS += -MMD -std=c++17 -fno-math-errno -fno-trapping-math -fPIC -D_OS_UNIX_ -DALVER=\"$(VER)\" $(CFLAGS)

	LDFLAGS += $(INCLUDES) -lm -pthread -lpthread -lstdc++ $(GUI_LDFLAGS) \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --libs sndfile fftw3 fftw3f $(BACKEND) `
	ifneq ($(MACOS)$(WINDOWS),true)
		LDFLAGS += -lrt -lc
	endif

	ifneq (,$(findstring clang, $(CXX)))
		CXXFLAGS := $(filter-out -fstrength-reduce,$(CXXFLAGS))
		CXXFLAGS += -Wno-unused-private-field
	endif

	# invoke build files
	OBJECTS = main.cpp

	DEPS = $(NAME)_app.d $(NAME)_lv2.d $(NAME)_clap.d $(NAME).d $(NAME)_ui.d

.PHONY : all 


all : check $(NAME)
	$(QUIET)mkdir -p ../bin
	$(QUIET)cp ./$(NAME)$(EXE) ../bin
	@if ! [ -f ../bin/$(NAME)$(EXE) ]; then \
		$(R_ECHO) "Sorry, build fail$(reset)"; \
	fi
	@$(B_ECHO) "=================== DONE =======================$(reset)"

clap: $(NAME).clap
	$(QUIET)mkdir -p ../bin/
	$(QUIET)cp ./$(NAME).clap ../bin/
	@$(B_ECHO) "=================== DONE =======================$(reset)"

lv2: $(NAME).$(LIB_EXT) $(NAME)_ui.$(LIB_EXT)
	@$(B_ECHO) "Create $(BUNDLE) $(reset)"
	$(QUIET)mkdir -p ../bin/$(BUNDLE)
	$(QUIET)cp $(LV2_DIR)*.ttl ../bin/$(BUNDLE)
	$(QUIET)$(TTLUPDATE)
	$(QUIET)$(TTLUPDATEGUI)
	$(QUIET)cp ./*.$(LIB_EXT) ../bin/$(BUNDLE)
	@if ! [ -f ../bin/$(BUNDLE)/$(NAME).$(LIB_EXT) ]; then \
		$(R_ECHO) "Sorry, build fail$(reset)"; \
	fi
	@$(B_ECHO) "=================== DONE =======================$(reset)"

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
	$(QUIET)rm -f *.o *.d *.a *.lib *.clap *.$(LIB_EXT)
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
	$(QUIET)cp ./Resources/$(NAME).svg $(DESTDIR)$(PIXMAPS_DIR)/$(NAME).svg
	$(QUIET)update-desktop-database || true
  endif
else
	@$(B_ECHO) "$(NAME)$(EXE) Standalone skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(PLUGNAME).clap)","")
	@$(B_ECHO) "Install  $(PLUGNAME).clap to $(DESTDIR)$(CLAP_INSTAL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(CLAP_INSTAL_DIR)/
	$(QUIET)cp -r ../bin/$(PLUGNAME).clap $(DESTDIR)$(CLAP_INSTAL_DIR)/$(PLUGNAME).clap
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

uninstall :
	$(QUIET)rm -rf $(DESTDIR)$(EXE_INSTALL_DIR)/$(NAME)
	$(QUIET)rm -rf $(DESTDIR)$(LV2_INSTALL_DIR)/$(BUNDLE)
	$(QUIET)rm -rf $(DESTDIR)$(CLAP_INSTAL_DIR)/$(PLUGNAME).clap

$(NAME) : $(OBJECTS)
	@$(B_ECHO) "Compiling $(NAME) $(reset)"
	$(QUIET)$(CXX) -MMD -MF $(NAME)_app.d $(CXXFLAGS) $(OBJECTS) -o $(NAME)$(EXE) $(LDFLAGS)
ifneq ($(MAKECMDGOALS),debug)
	$(QUIET)$(STRIP) -s -x -X -R .note.ABI-tag $(NAME)$(EXE)
endif

$(NAME).clap: $(CLAP_SOURCES) $(CLAP_DIR)ToneShiftEQ.cc
	@$(B_ECHO) "Compiling $(NAME).clap $(reset)"
	$(QUIET)$(CXX) -shared -MMD -MF $(NAME)_clap.d $(CXXFLAGS) $(CLAP_INCLUDE) $(CLAP_SOURCES) -o $(NAME).clap  $(LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME).clap
endif

$(NAME)_ui.$(LIB_EXT): $(LV2_GUI).cpp $(UI_LIB)
	@$(B_ECHO) "Compiling $(NAME)_ui.$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) -shared -MMD -MF $(NAME)_ui.d $(CXXFLAGS) -I./ $(LV2_INCLUDE) $(LV2_GUI).cpp -L. $(UI_LIB) -o $@ $(LDFLAGS) $(GUI_LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME)_ui.$(LIB_EXT)
endif

$(NAME).$(LIB_EXT): $(LV2_PLUGIN).cpp
	@$(B_ECHO) "Compiling $(NAME).$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) -shared -MMD -MF $(NAME)_lv2.d $(CXXFLAGS) $(LV2_INCLUDE) $(LV2_PLUGIN).cpp -o $(NAME).$(LIB_EXT) $(LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME).$(LIB_EXT)
endif

doc:
	#pass
