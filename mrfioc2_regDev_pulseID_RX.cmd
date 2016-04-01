# Initialize the reg dev to data buffer connection (receive only first 10 segments = 160 bytes)
mrfioc2_regDevConfigure $(NAME=$(DEVICE=EVR0)DBUF) $(DEVICE=EVR0) $(PROTOCOL=0) $(USER_OFFSET=16) $(MAX_LENGTH=160)

## Data buffer receive template for pulse ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/pulseId_RX.template", "P=$(SYS)-$(DEVICE=EVR0):, NAME=$(NAME=$(DEVICE=EVR0)DBUF)")
