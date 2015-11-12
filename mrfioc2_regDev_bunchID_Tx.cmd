# used to set the timstamp for the SEC and NSEC records
require settimestamp

# Initialize the reg dev to data buffer connection
mrfiocDBuffConfigure $(EVG=EVG0)DBUFF $(EVG=EVG0) 1

## Data buffer receive template for bunch ID
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/bunchId_Tx.template", "SYS=$(SYS)-DBUF,EVG=$(EVG=EVG0),EVGDBUFF=$(EVG=EVG0)DBUFF")
