# Specify T_A so we can use DB_INSTALLS for installing camera specific
# templates and env files.  No need to do target specific builds
T_A=$(EPICS_HOST_ARCH)
TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# In a Diamond Support Module, install
# templates like this
#
DB_INSTALLS += $(wildcard *.template)

# Also install env files 
DB_INSTALLS += $(wildcard *.env)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

