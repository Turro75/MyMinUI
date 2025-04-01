#!/bin/sh

TOGGLEFILENAME="${USERDATA_PATH}/is-rg353p"

if [ -f $TOGGLEFILENAME ]; then
rm -rf $TOGGLEFILENAME
else
echo " " > $TOGGLEFILENAME
fi

