require settimestamp

# Initialize the reg dev to data buffer connection
mrfioc2_regDevConfigure $(NAME=$(DEVICE=EVR0)DBUF$(TYPE=300)) $(DEVICE=EVR0) $(TYPE=300) $(PROTOCOL=0) $(USER_OFFSET=16)

## Data buffer receive template for pulse ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/pulseId_RX.template", "P=$(SYS)-$(DEVICE=EVR0):, NAME=$(NAME=$(DEVICE=EVR0)DBUF$(TYPE=300)), ID=$(ID=), MTS_FLNK=$(MTS_FLNK=)")
