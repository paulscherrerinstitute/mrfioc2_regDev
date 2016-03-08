#!/bin/bash
set -o errexit

#Set caqtdm display path - to retrieve the stylesheet file
export CAQTDM_DISPLAY_PATH=/sf/op/config/qt:/sf/rf/config/qt:$CAQTDM_DISPLAY_PATH

SYS=""
DEVICE="EVR0"
NAME="EVR0DBUF"
SCREEN="G_TI_BUNCHID.ui"

usage()
{
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "    -s <system name>     (required) The system/project name"
    echo "    -d <DEVICE name>     Timing card name (default: $DEVICE)"
    echo "    -n <regDev name>     regDev name (default: $NAME)"
    echo "    -h                   This help"
}

s_flag=0 # reset s_flag (-s is required)

while getopts ":s:d:n:h" o; do
    case "${o}" in
        s)
            SYS=${OPTARG}
            s_flag=1
            ;;
        d)
            DEVICE=${OPTARG}
            ;;

        n)
            NAME=${OPTARG}
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

macro="SYS=$SYS,NAME=$NAME,DEVICE=$DEVICE"

#'startDM' should be replaced with 'caqtdm' once the new version of caqtdm is out.
startDM -stylefile sfop.qss -macro "$macro" $SCREEN &


