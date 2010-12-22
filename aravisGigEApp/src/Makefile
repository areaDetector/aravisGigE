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

# We need to link against aravis
USR_INCLUDES += -I/dls_sw/work/common/glibnew/lib/glib-2.0/include
USR_INCLUDES += -I/dls_sw/work/common/glibnew/include/glib-2.0
USR_INCLUDES += -I$(TOP)/include/aravis
USR_LIBS += glib-2.0
USR_LIBS += aravis
glib-2.0_DIR = /dls_sw/work/common/glibnew/lib
aravis_DIR = /dls_sw/work/common/glibnew/lib

include $(TOP)/configure/RULES
