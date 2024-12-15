#!/bin/sh

TOGGLEFILENAME="${SHARED_USERDATA_PATH}/hide-state"

if [ -f $TOGGLEFILENAME ]; then
rm -rf $TOGGLEFILENAME
else
echo " " > $TOGGLEFILENAME
fi

