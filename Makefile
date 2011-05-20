#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
install: $(TOP)/aravis/Makefile

DIRS := $(DIRS) $(filter-out $(DIRS), aravis)
DIRS := $(DIRS) $(filter-out $(DIRS), configure)
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *App))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocBoot))

define DIR_template
 $(1)_DEPEND_DIRS = configure
endef
$(foreach dir, $(filter-out configure,$(DIRS)),$(eval $(call DIR_template,$(dir))))

iocBoot_DEPEND_DIRS += $(filter %App,$(DIRS))

# Comment out the following line to creation of example iocs and documentation
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard etc))
# Comment out the following line to disable building of example iocs
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocs))
include $(TOP)/configure/RULES_TOP

# this makes aravis
FULLTOP=$(shell pwd)
CONFIGOPTIONS = --bindir=$(FULLTOP)/bin/$(EPICS_HOST_ARCH)
CONFIGOPTIONS += --libdir=$(FULLTOP)/lib/$(EPICS_HOST_ARCH)  
CONFIGOPTIONS += --includedir=$(FULLTOP)/include
CONFIGOPTIONS += --with-html-dir=$(FULLTOP)/documentation
CONFIGOPTIONS += --prefix=/tmp/aravis-install
CONFIGOPTIONS += --enable-gst-plugin --enable-viewer 
#CONFIGOPTIONS += --disable-nls --disable-cairo --disable-gtk-doc-html --enable-viewer
ENVEXPORTS =
ifneq ($(GLIBPREFIX),)
	ENVEXPORTS += export PKG_CONFIG_PATH=$(GLIBPREFIX)/lib/pkgconfig;
endif
$(TOP)/aravis/Makefile:
	(cd aravis; $(ENVEXPORTS) sh configure $(CONFIGOPTIONS))
