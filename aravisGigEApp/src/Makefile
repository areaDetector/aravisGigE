TOP=../..

include $(TOP)/configure/CONFIG

# -------------------------------
# Build an Diamond Support Module
# -------------------------------

# Build some little helper functions
PROD_IOC += dumpXml
dumpXml_SRCS_DEFAULT += dumpXml.c
dumpXml_LIBS += $(EPICS_BASE_IOC_LIBS)

LIBRARY_IOC += aravisCamera

# The following are compiled and added to the support library
aravisCamera_SRCS += aravisCamera.cpp

DBD += aravisCameraSupport.dbd

# If glib is not in a standard place, tell the build system where to look
ifneq ($(GLIBPREFIX),)
	USR_INCLUDES += -I$(GLIBPREFIX)/lib/glib-2.0/include
	USR_INCLUDES += -I$(GLIBPREFIX)/include/glib-2.0
	glib-2.0_DIR = $(GLIBPREFIX)/lib
endif	

# We need to link against aravis
USR_INCLUDES += -I$(TOP)/include/aravis
USR_LIBS += glib-2.0
USR_LIBS += aravis

include $(TOP)/configure/RULES
