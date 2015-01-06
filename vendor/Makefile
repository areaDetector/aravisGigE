TOP = ..
include $(TOP)/configure/CONFIG

# config options for aravis
CONFIGOPTIONS = --prefix=$(shell cd $(TOP)/vendor && pwd)/aravis-linux-x86_64
CONFIGOPTIONS += --disable-gst-plugin --disable-viewer

# libs to install
ARAVIS_LINUX_LIBS = libaravis-0.4.a libaravis-0.4.so.0.0.0 libaravis-0.4.so.0 libaravis-0.4.so
ARAVIS_LINUX_BINS = arv-tool-0.4

# export path for GLIB if GLIBPREFIX 
ENVEXPORTS =
ifneq ($(GLIBPREFIX),)
    ifneq (,$(wildcard $(GLIBPREFIX)/lib64/glib-2.0))
	    # If these is a glibprefix/lib64/glib-2.0 dir then use this	    
		ENVEXPORTS += export PKG_CONFIG_PATH=$(GLIBPREFIX)/lib64/pkgconfig;
    else
        # Assume it is in the lib directory
		ENVEXPORTS += export PKG_CONFIG_PATH=$(GLIBPREFIX)/lib/pkgconfig;
	endif	
endif

# install libs
# TODO: shouldn't really do this, should get the aravisGgigeApp/src/Makefile to point to the vendor directory
BIN_INSTALLS_Linux  += $(addprefix ../aravis-linux-x86_64/bin/, $(ARAVIS_LINUX_BINS))
LIB_INSTALLS_Linux  += $(addprefix ../aravis-linux-x86_64/lib/, $(ARAVIS_LINUX_LIBS))
ARAVISMAK = $(TOP)/vendor/aravis-linux-x86_64/Makefile	

# depend on Makefile existing
install: $(ARAVISMAK)

include $(TOP)/configure/RULES

$(TOP)/vendor/aravis-%/Makefile:
	(mkdir -p $(TOP)/vendor/aravis-$(*F); cd $(TOP)/vendor/aravis-$(*F); $(ENVEXPORTS) sh ../aravis/configure $(CONFIGOPTIONS) && make && make install && chmod -R g+w .)
