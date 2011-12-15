#Makefile at top of application tree
TOP = .

include $(TOP)/configure/CONFIG
DIRS := $(DIRS) $(filter-out $(DIRS), configure)
DIRS := $(DIRS) $(filter-out $(DIRS), vendor)
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *App))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocBoot))

define DIR_template
 $(1)_DEPEND_DIRS = configure
endef
$(foreach dir, $(filter-out configure,$(DIRS)),$(eval $(call DIR_template,$(dir))))

iocBoot_DEPEND_DIRS += $(filter %App,$(DIRS))

# make sure examples are only built on linux-x86
ifeq ($(EPICS_HOST_ARCH), linux-x86)
	# Comment out the following lines to disable creation of example iocs and documentation
	DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard etc))
	ifeq ($(wildcard etc),etc)
		include $(TOP)/etc/makeIocs/Makefile.iocs
		UNINSTALL_DIRS += documentation/doxygen $(IOC_DIRS)
	endif
endif

# Comment out the following line to disable building of example iocs
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocs))
include $(TOP)/configure/RULES_TOP

