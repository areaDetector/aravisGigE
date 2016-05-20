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

# Set aravis include and lib paths
ifeq ($(ARAVISPREFIX),)
	ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
		ARAVISPREFIX = $(TOP)/vendor/aravis-linux-x86_64
	else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))
		ARAVISPREFIX = $(TOP)/vendor/aravis-linux-x86
	endif
endif
ifneq ($(ARAVISPREFIX),)
	USR_INCLUDES += -I$(ARAVISPREFIX)/include/aravis-0.4
endif

USR_LIBS += glib-2.0

# TODO: should pick this up from the vendor directory
USR_LIBS += aravis-0.4

# Install all libs to $(TOP)/lib/$(T_A) so ioc builds can find them easily
LIB_INSTALLS += $(wildcard $(ARAVISPREFIX)/lib/lib*)
LIB_INSTALLS += $(wildcard $(GLIBPREFIX)/lib/lib*)

include $(TOP)/configure/RULES
