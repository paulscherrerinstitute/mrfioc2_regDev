# Initialize the reg dev to data buffer connection
mrfioc2_regDevConfigure $(EVR=EVR0)DBUFF $(EVR=EVR0) 0 16 32

## Data buffer receive template for bunch ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/bunchId_Rx.template", "SYS=$(SYS)-DBUF,EVR=$(EVR=EVR0),EVRDBUFF=$(EVR=EVR0)DBUFF")
