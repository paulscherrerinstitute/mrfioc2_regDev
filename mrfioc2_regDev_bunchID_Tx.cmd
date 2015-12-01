# used to set the timstamp for the SEC and NSEC records
require settimestamp

# Initialize the reg dev to data buffer connection
mrfioc2_regDevConfigure $(NAME=$(DEVICE=EVG0)DBUF) $(DEVICE=EVG0) $(PROTOCOL=0) $(USER_OFFSET=16) $(MAX_LENGTH=32)

## Data buffer receive template for bunch ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/bunchId_Tx.template", "SYS=$(SYS)-DBUF, DEVICE=$(DEVICE=EVG0), NAME=$(NAME=$(DEVICE=EVG0)DBUF)")
