TOP=../..

include $(TOP)/configure/CONFIG

# -------------------------------
# Build an Diamond Support Module
# -------------------------------

# Build some little helper functions
#PROD_IOC += aravisTest
#aravisTest_SRCS_DEFAULT += aravisTest.c
#aravisTest_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC += inspectcam
inspectcam_SRCS += inspectcam.cpp

TESTPROD_IOC += testcam
testcam_SRCS += testcam.cpp
testcam_LIBS += aravisCamera ADBase asyn dbCore Com

LIBRARY_IOC += aravisCamera

# The following are compiled and added to the support library
aravisCamera_SRCS += aravisCamera.cpp

DBD += aravisCameraSupport.dbd

# If glib is not in a standard place, tell the build system where to look
ifneq ($(GLIBPREFIX),)
	USR_INCLUDES += -I$(GLIBPREFIX)/include/glib-2.0
    ifneq (,$(wildcard $(GLIBPREFIX)/lib64/glib-2.0))
	    # If these is a glibprefix/lib64/glib-2.0 dir then use this	    
		USR_INCLUDES += -I$(GLIBPREFIX)/lib64/glib-2.0/include
    	glib-2.0_DIR = $(GLIBPREFIX)/lib64
    else
        # Assume it is in the lib directory
		USR_INCLUDES += -I$(GLIBPREFIX)/lib/glib-2.0/include		
		glib-2.0_DIR = $(GLIBPREFIX)/lib
	endif
endif

USR_INCLUDES +=  -I$(GLIB_INC1)
USR_INCLUDES +=  -I$(GLIB_INC2)

# We need to link against aravis
ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86_64/include/aravis-0.4
else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86/include/aravis-0.4
endif

USR_LIBS += glib-2.0

# TODO: should pick this up from the vendor directory
USR_LIBS += aravis-0.4

include $(TOP)/configure/RULES
