TOP = ../..
include $(TOP)/configure/CONFIG

# this is the dls-xml-iocbuilder.py file
XMLBUILDER := dls-xml-iocbuilder.py
XMLBUILDER := /dls_sw/work/common/python/iocbuilder/xmlbuilder/xmlbuilder.py

# set to -d to get debugging
DEBUG := 

# Create an IOC from all .xml files and all .py files
IOCS := $(patsubst %.xml, %, $(wildcard *.xml)) $(patsubst %.py, %, $(wildcard *.py))

# These are the IOCS we don't want to make simulations out of
NO_SIMS :=

# Make simulations from all IOCS not in NO_SIMS
SIMS := $(patsubst %, %_sim, $(filter-out $(NO_SIMS), $(IOCS)))

# These are the IOC dirs to make
IOC_DIRS := $(patsubst %, $(TOP)/iocs/%, $(IOCS) $(SIMS))

# Add the created iocs and sims to the install target
install: $(IOC_DIRS)

# General rule for building a Standard IOC from an XML file
$(TOP)/iocs/%: %.xml
	$(XMLBUILDER) $(DEBUG) -e $<

# General rule for building a Simulation IOC from an XML file
$(TOP)/iocs/%_sim: %.xml
	$(XMLBUILDER) $(DEBUG) --sim=linux-x86 -e $<

# Alternate rule for building a Standard IOC from a python file
$(TOP)/iocs/%: %.py
	./$< $(DEBUG) $*

# Alternate rule for building a Simulation IOC from a python file
$(TOP)/iocs/%_sim: %.py
	./$< $(DEBUG) --sim=linux-x86 $*

# Remove the generated iocs on clean
clean:
	rm -rf $(IOC_DIRS)
