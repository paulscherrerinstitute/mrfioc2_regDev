include /ioc/tools/driver.makefile

MODULE=mrfioc2_regDev
#LIBVERSION=test


BUILDCLASSES += Linux
EXCLUDE_VERSIONS=3.13 3.14.8
ARCH_FILTER=eldk52-e500v2 eldk42-ppc4xxFP SL%


SOURCES += src/drvMrfiocDBuff.cpp

DBDS += src/mrfregdev.dbd

TEMPLATES += templates/bunchId_Rx.template
TEMPLATES += templates/bunchId_Tx.template
TEMPLATES += template/ai.template

# If there is no numbered version of the mrfioc2 driver available
# you can specify which version to build agains here:
#mrfioc2_VERSION=skube_s