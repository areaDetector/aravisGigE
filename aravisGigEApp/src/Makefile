TOP=../..

include $(TOP)/configure/CONFIG

# -------------------------------
# Build an Diamond Support Module
# -------------------------------

# Build some little helper functions
#PROD_IOC += aravisTest
#aravisTest_SRCS_DEFAULT += aravisTest.c
#aravisTest_LIBS += $(EPICS_BASE_IOC_LIBS)

LIBRARY_IOC += aravisCamera

# The following are compiled and added to the support library
aravisCamera_SRCS += aravisCamera.cpp

DBD += aravisCameraSupport.dbd

# If glib is not in a standard place, tell the build system where to look
ifneq ($(GLIBPREFIX),)
	USR_INCLUDES += -I$(GLIBPREFIX)/lib/glib-2.0/include
	USR_INCLUDES += -I$(GLIBPREFIX)/include/glib-2.0
	ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
		glib-2.0_DIR = $(GLIBPREFIX)/lib64
	else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))
		glib-2.0_DIR = $(GLIBPREFIX)/lib
	endif	
endif	

# We need to link against aravis
ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86_64/include/aravis-0.2
else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86/include/aravis-0.2
endif

USR_LIBS += glib-2.0
USR_LIBS += aravis-0.2

include $(TOP)/configure/RULES
