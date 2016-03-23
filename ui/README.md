# Pulse ID readout of the EVR
## Quick start
run 

    . ./start_TimDBuff.sh -s <system name>
from this folder.

Usage: $0 [options]

Options:
- -s <system name>     (required) System name (ie FIN).
- -d <EVR name>        Timing card name (ie EVR0) - (default: EVR0).
- -m <signal mode>     For Data Buffer Transmission mode use 'TX' - (default: RX).
- -h                   This help


## Files description
- __G_TI_PULSEID.ui__  Qt Panel that displays the Pulse ID data from timing card data buffer.
