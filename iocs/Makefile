TOP = ..
include $(TOP)/configure/CONFIG

ifeq ($(EPICS_HOST_ARCH), linux-x86)
	DIRS := $(wildcard example testCameras)
else ifeq ($(EPICS_HOST_ARCH), linux-x86_64)
	DIRS := $(wildcard example testCameras)
endif

include $(TOP)/configure/RULES_DIRS
