require settimestamp
require histogram
require SynApps

# Initialize the reg dev to data buffer connection
mrfioc2_regDevConfigure $(NAME=$(DEVICE=EVR0)DBUF$(TYPE=300)) $(DEVICE=EVR0) $(TYPE=300) $(PROTOCOL=0) $(USER_OFFSET=16)

## Data buffer receive template for pulse ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/pulseId_RX.template", "P=$(SYS)-$(DEVICE=EVR0):, NAME=$(NAME=$(DEVICE=EVR0)DBUF$(TYPE=300)), ID=$(ID=), PULSEID_FLNK=$(PULSEID_FLNK=), MTS_FLNK=$(MTS_FLNK=), UINTCNT_FLNK=$(UINTCNT_FLNK=), PIDL_FLNK=$(PIDL_FLNK=), PIDH_FLNK=$(PIDH_FLNK=)")
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/perf.template", "P=$(SYS)-$(DEVICE=EVR0):, ID=$(ID=), MODE=RX, AUTORST_TIME=$(PERF_AUTORST_TIME=60)")
