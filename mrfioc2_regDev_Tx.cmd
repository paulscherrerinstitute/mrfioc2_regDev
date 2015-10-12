epicsEnvSet EV $(EVG=$(EVR=EVR0))
## Data buffer transmit template for custom data
dbLoadRecords("$($(MODULE)_TEMPLATES)/$(RECORD_TYPE=ao).template", "SYS=$(SYS)-DBUF,EV=$(EV),ID=$(ID),OFFSET=$(OFFSET),PARAMS=$(PARAMS=T=int16)")
