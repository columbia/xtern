#!/bin/bash

nrun=$1
i=1
while [ $i -le $nrun ]
do
  echo $i
  make $2
  mv out out_$i
  i=`expr $i + 1`
  sleep 1
done
