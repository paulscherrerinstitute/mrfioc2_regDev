ifeq ($(wildcard /ioc/tools/driver.makefile),)
$(warning It seems you do not have the PSI build environment. Remove GNUmakefile.)
include Makefile
else
include /ioc/tools/driver.makefile

MODULE=mrfioc2_regDev
#LIBVERSION=test


BUILDCLASSES += Linux
EXCLUDE_VERSIONS=3.13 3.14.8
ARCH_FILTER=eldk52-e500v2 eldk42-ppc4xxFP SL% RHEL7-x86_64

SOURCES += src/mrfioc2_regDev.cpp

DBDS += src/mrfioc2_regDev.dbd

TEMPLATES += template/pulseId_RX.template
TEMPLATES += template/pulseId_TX.template


# If there is no numbered version of the mrfioc2 driver available
# you can specify which version to build agains here:
#mrfioc2_VERSION=skube_s
endif
