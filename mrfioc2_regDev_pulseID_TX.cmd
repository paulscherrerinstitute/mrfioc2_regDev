# used to set the timstamp for the SEC and NSEC records
require settimestamp

# Initialize the reg dev to data buffer connection
mrfioc2_regDevConfigure $(NAME=$(DEVICE=EVG0)DBUF$(TYPE=300)) $(DEVICE=EVG0) $(TYPE=300) $(PROTOCOL=0) $(USER_OFFSET=16)

## Data buffer receive template for pulse ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/pulseId_TX.template", "P=$(SYS)-$(DEVICE=EVG0):, NAME=$(NAME=$(DEVICE=EVG0)DBUF$(TYPE=300)), ID=$(ID=)")
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/perf.template", "P=$(SYS)-$(DEVICE=EVG0):, ID=$(ID=), MODE=TX")
