# Initialize the reg dev to data buffer connection
mrfioc2_regDevConfigure $(NAME=$(DEVICE=EVR0)DBUFF) $(DEVICE=EVR0) $(PROTOCOL=0) $(USER_OFFSET=16) $(MAX_LENGTH=32)

## Data buffer receive template for bunch ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/bunchId_Rx.template", "SYS=$(SYS),DEVICE=$(DEVICE=EVR0), NAME=$(NAME=$(DEVICE=EVR0)DBUFF)")
