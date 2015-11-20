# used to set the timstamp for the SEC and NSEC records
require settimestamp

# Initialize the reg dev to data buffer connection
mrfioc2_regDevConfigure $(EVG=EVG0)DBUFF $(EVG=EVG0) 0 32 16

## Data buffer receive template for bunch ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/bunchId_Tx.template", "SYS=$(SYS)-DBUF,EVG=$(EVG=EVG0),EVGDBUFF=$(EVG=EVG0)DBUFF")
