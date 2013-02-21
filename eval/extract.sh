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

find . -name stats.txt -print | sort | xargs -I {} bash -c \
"awk '
BEGIN{line=0; dthreads=0; dmp_o=0; dmp_b=0; dmp_pb=0; dmp_hb=0;}
{
    if (\$1 == \"dthreads:\") dthreads = 1;
    if (\$1 == \"dmp_o:\") dmp_o = 1;
    if (\$1 == \"dmp_b:\") dmp_b = 1;
    if (\$1 == \"dmp_pb:\") dmp_pb = 1;
    if (\$1 == \"dmp_hb:\") dmp_hb = 1;
    line++;
    if (line == 4) xtern_avg = \$2;
    if (line == 5) xtern_sem = \$2;
    if (line == 7) nondet_avg = \$2;
    if (line == 8) nodet_sem = \$2;
    if (line == 8){print nondet_avg, nodet_sem, xtern_avg, xtern_sem;}
}
END {
    print dthreads, dmp_o, dmp_b, dmp_pb, dmp_hb
}
' {}; "

