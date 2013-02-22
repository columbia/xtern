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
BEGIN{line=0;
      dthreads=0; dmp_o=0; dmp_b=0; dmp_pb=0; dmp_hb=0;
      avg_count=0; sem_count=0}
{
    if (\$1 == \"dthreads:\") dthreads = 1;
    if (\$1 == \"dmp_o:\") dmp_o = 1;
    if (\$1 == \"dmp_b:\") dmp_b = 1;
    if (\$1 == \"dmp_pb:\") dmp_pb = 1;
    if (\$1 == \"dmp_hb:\") dmp_hb = 1;
    if (\$1 == \"avg\") {
        avg[avg_count] = \$2;
        ++avg_count;
    }
    if (\$1 == \"sem\") {
        sem[sem_count] = \$2;
        ++sem_count;
    }
    #line++;
    #if (line == 4) xtern_avg = \$2;
    #if (line == 5) xtern_sem = \$2;
    #if (line == 7) nondet_avg = \$2;
    #if (line == 8) nodet_sem = \$2;
    #if (line == 8){print nondet_avg, nodet_sem, xtern_avg, xtern_sem;}
}
END {
    printf \"%s %.10f %s %.10f\", avg[2], sem[1], avg[1], sem[0]
    avg_count = 4
    sem_count = 2
    if (dthreads == 1) {
        printf \" %s %.10f\", avg[avg_count], sem[sem_count];
        avg_count += 2;
        sem_count += 1;
    }
    else
        printf \"  \";
    if (dmp_o== 1) {
        printf \" %s %.10f\", avg[avg_count], sem[sem_count];
        avg_count += 2;
        sem_count += 1;
    }
    else
        printf \"  \";
    if (dmp_b == 1) {
        printf \" %s %.10f\", avg[avg_count], sem[sem_count];
        avg_count += 2;
        sem_count += 1;
    }
    else
        printf \"  \";
    if (dmp_pb == 1) {
        printf \" %s %.10f\", avg[avg_count], sem[sem_count];
        avg_count += 2;
        sem_count += 1;
    }
    else
        printf \"  \";
    if (dmp_hb == 1) {
        printf \" %s %.10f\", avg[avg_count], sem[sem_count];
        avg_count += 2;
        sem_count += 1;
    }
    else
        printf \"  \";

    print \"\"
}
' {}; "

