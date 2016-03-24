#!/bin/bash
set -o errexit

#Set caqtdm display path - to retrieve the stylesheet file
export CAQTDM_DISPLAY_PATH=/sf/op/config/qt:$CAQTDM_DISPLAY_PATH

SYS=""
DEVICE="EVR0"
DBUF="DBUF"
SCREEN="G_TI_PULSEID.ui"
MODE="RX"

usage()
{
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "    -s <system name>     (required) System name (ie FIN)."
    echo "    -d <DEVICE name>     Timing card name (ie EVR0) - (default: $DEVICE)."
    echo "    -m <mode>     	   For Data Buffer Transmission mode use 'Tx' - (default: $MODE)."
    echo "    -b <DBUF name>     Unique DBUF identifier (eg. DBUF) - (default: $DBUF)."
    echo "    -h                   This help."
}

s_flag=0 # reset s_flag (-s is required)

while getopts ":s:d:m:h" o; do
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
            
       	b)
            DBUF=${OPTARG}
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

macro="P=$SYS-$DEVICE-$DBUF:,MODE=$MODE"

#'startDM' should be replaced with 'caqtdm' once the new version of caqtdm is out.
startDM -stylefile sfop.qss -macro "$macro" $SCREEN &


