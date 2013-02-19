#!/bin/bash

if [ ! -d $XTERN_ROOT ]; then
    echo "XTERN_ROOT is not defined"
    exit 1
fi

if [ "$1" == "" ]; then
    logs=$XTERN_ROOT/eval/current
else
    logs=$1
fi

if [ ! -d $logs ]; then
    echo "$logs is not a folder!"
    exit 1
fi
cd $logs

find . -name stats.txt -print | sort | cut -d "/" -f 2 

find . -name stats.txt -print | sort | xargs -I {} bash -c "awk 'BEGIN{line=0;}{line++;if (line == 4)xtern_avg = \$2;if (line == 5)xtern_sem = \$2;if (line == 7)nondet_avg = \$2;if (line==8){print nondet_avg, \$2, xtern_avg, xtern_sem;line = 0;}}' {}; "

