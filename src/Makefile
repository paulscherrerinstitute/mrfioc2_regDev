TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC support library

RELEASE_INCLUDES += -I$(INSTALL_LOCATION)/include/dataBuffer

LIBRARY_IOC += mrfioc2_regDev

mrfioc2_regDev_SRCS += mrfioc2_regDev.cpp

DBD += mrfioc2_regDev.dbd

mrfioc2_regDev_LIBS += regDev

ifeq ($(OS),Windows_NT)
mrfioc2_regDev_LIBS += mrmShared $(EPICS_BASE_IOC_LIBS)
endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

