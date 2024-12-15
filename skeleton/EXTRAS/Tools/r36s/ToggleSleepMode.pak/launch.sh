#!/bin/sh

TOGGLEFILENAME="${SHARED_USERDATA_PATH}/enable-sleep-mode"

if [ -f $TOGGLEFILENAME ]; then
rm -rf $TOGGLEFILENAME
else
echo " " > $TOGGLEFILENAME
fi

