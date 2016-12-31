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

USR_INCLUDES +=  $(addprefix -I, $(GLIB_INCLUDE))

# We need to link against aravis
ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86_64/include/aravis-0.6
else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86/include/aravis-0.6
endif

USR_LIBS += glib-2.0

# TODO: should pick this up from the vendor directory
USR_LIBS += aravis-0.6

PROD_LDLIBS += aravis-0.6 glib-2.0

include $(TOP)/configure/RULES
