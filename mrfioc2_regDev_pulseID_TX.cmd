# used to set the timstamp for the SEC and NSEC records
require settimestamp

# Initialize the reg dev to data buffer connection (send only first 10 segments = 160 bytes)
mrfioc2_regDevConfigure $(NAME=$(DEVICE=EVG0)DBUF$(TYPE=1)) $(DEVICE=EVG0) $(TYPE=1) $(PROTOCOL=0) $(USER_OFFSET=16)

## Data buffer receive template for pulse ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/pulseId_TX.template", "P=$(SYS)-$(DEVICE=EVG0):, NAME=$(NAME=$(DEVICE=EVG0)DBUF$(TYPE=1))")
