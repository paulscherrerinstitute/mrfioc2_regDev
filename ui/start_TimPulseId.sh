#!/bin/bash
set -o errexit

SYS=""
DEVICE="EVR0"
SCREEN="G_TI_PULSEID.ui"
MODE="RX"
ATTACH="-attach"
ID=""

usage()
{
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "    -s <system name>     (required) System name (ie STEST-VME-123)."
    echo "    -d <DEVICE name>     Timing card name (ie EVR0) - (default: $DEVICE)."
    echo "    -m <mode>     	   For Data Buffer Transmission mode use 'TX' - (default: $MODE)."
    echo "    -i <ID>              ID used in startup script when instantionating records - (default: empty)."
    echo "    -n                   Do not attach to existing caQtDM. Open new one instead"
    echo "    -h                   This help."
}

s_flag=0 # reset s_flag (-s is required)

while getopts ":s:d:m:i:nh" o; do
    case "${o}" in
        s)
            SYS=${OPTARG}
            s_flag=1
            ;;
        d)
            DEVICE=${OPTARG}
            ;;
        m)
            MODE=${OPTARG}
            ;;
        i)
            ID=${OPTARG}
            ;;
        n)
            ATTACH=""
            ;;
        h)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done

# -s is not present
if [ $s_flag -eq 0 ]; then
    usage
    exit 1
fi

macro="P=$SYS-$DEVICE:,MODE=$MODE$ID"

#'startDM' should be replaced with 'caqtdm' once the new version of caqtdm is out.
caqtdm -stylefile /sf/op/config/qt/sfop.qss $ATTACH -macro "$macro" $SCREEN &


