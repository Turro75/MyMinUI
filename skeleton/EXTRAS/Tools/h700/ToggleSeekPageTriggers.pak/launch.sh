#!/bin/sh

TOGGLEFILENAME="${SHARED_USERDATA_PATH}/enable-seek-triggers"

if [ -f $TOGGLEFILENAME ]; then
rm -rf $TOGGLEFILENAME
else
echo " " > $TOGGLEFILENAME
fi

