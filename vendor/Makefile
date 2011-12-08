TOP = ..
include $(TOP)/configure/CONFIG

# config options for aravis
CONFIGOPTIONS = --prefix=$(shell cd $(TOP)/vendor && pwd)/aravis-$(T_A)
CONFIGOPTIONS += --enable-gst-plugin --enable-viewer 

# libs to install
ARAVIS_LINUX_LIBS = libaravis-0.2.a libaravis-0.2.so.0.0.0 libaravis-0.2.so.0 libaravis-0.2.so
ARAVIS_LINUX_BINS = arv-tool-0.2

# export path for GLIB if GLIBPREFIX 
ENVEXPORTS =
ifneq ($(GLIBPREFIX),)
	ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
		ENVEXPORTS += export PKG_CONFIG_PATH=$(GLIBPREFIX)/lib64/pkgconfig;
	else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))		
		ENVEXPORTS += export PKG_CONFIG_PATH=$(GLIBPREFIX)/lib/pkgconfig;
	endif	
endif

# install libs
ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
	BIN_INSTALLS_Linux  += $(addprefix ../aravis-linux-x86_64/bin/, $(ARAVIS_LINUX_BINS))
	LIB_INSTALLS_Linux  += $(addprefix ../aravis-linux-x86_64/lib/, $(ARAVIS_LINUX_LIBS))
	ARAVISMAK = $(TOP)/vendor/aravis-linux-x86_64/Makefile	
else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))
	BIN_INSTALLS_Linux  += $(addprefix ../aravis-linux-x86/bin/, $(ARAVIS_LINUX_BINS))
	LIB_INSTALLS_Linux  += $(addprefix ../aravis-linux-x86/lib/, $(ARAVIS_LINUX_LIBS))
	ARAVISMAK = $(TOP)/vendor/aravis-linux-x86/Makefile
endif

# depend on Makefile existing
install: $(ARAVISMAK)

include $(TOP)/configure/RULES

$(TOP)/vendor/aravis-%/Makefile:
	(mkdir -p $(TOP)/vendor/aravis-$(*F); cd $(TOP)/vendor/aravis-$(*F); $(ENVEXPORTS) sh ../aravis/configure $(CONFIGOPTIONS) && make && make install)
