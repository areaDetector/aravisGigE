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

FULLTOP=$(shell pwd)
CONFIGOPTIONS = --bindir=$(FULLTOP)/bin/$(EPICS_HOST_ARCH)
CONFIGOPTIONS += --libdir=$(FULLTOP)/lib/$(EPICS_HOST_ARCH)
CONFIGOPTIONS += --datarootdir=$(FULLTOP)/doc
CONFIGOPTIONS += --prefix=$(FULLTOP)
#CONFIGOPTIONS += --disable-gtk-doc-html --disable-nls --disable-gstreamer --disable-cairo

$(TOP)/aravis/Makefile:
	(cd aravis; export PKG_CONFIG_PATH=/dls_sw/work/common/glibnew/lib/pkgconfig; export PATH=${PATH}:/dls_sw/work/common/glibnew/bin; sh configure $(CONFIGOPTIONS))
