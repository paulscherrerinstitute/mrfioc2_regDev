## Data buffer receive template for custom data
dbLoadRecords("$($(MODULE)_TEMPLATES)/$(RECORD_TYPE=ai).template", "SYS=$(SYS)-DBUF,EV=$(EVR=EVR0),ID=$(ID),OFFSET=$(OFFSET),PARAMS=$(PARAMS=T=int16)")
