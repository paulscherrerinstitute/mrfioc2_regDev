TOP=.

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC support library

LIBRARY_IOC += mrfioc2_regDev

mrfioc2_regDev_SRCS += drvMrfiocDBuff.cpp

DBD += mrfregdev.dbd

mrfioc2_regDev_LIBS += regDev

ifeq ($(OS),Windows_NT)
mrfioc2_regDev_LIBS += evrMrm $(EPICS_BASE_IOC_LIBS)
endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

