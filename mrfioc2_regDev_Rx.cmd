## Data buffer receive template for custom data
dbLoadRecords("$(mrfioc2_regDev_TEMPLATES)/$(RECORD_TYPE=ai).template", "SYS=$(SYS)-DBUF,EV=$(EVR=EVR0),ID=$(ID),OFFSET=$(OFFSET),PARAMS=$(PARAMS=T=int16)")
