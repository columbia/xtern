#!/bin/bash

if [ -z $1 ]; then
        echo "Usage: <source file name: e.g., ./build tr.c>"
        exit 1
fi

SRC=$1
PROG=`echo $SRC | sed 's/\(.*\)\..*/\1/'`
llgcc $SRC -o $PROG
