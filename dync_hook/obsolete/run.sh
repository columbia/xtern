#!/bin/bash

path=$PWD
echo $path
cd $XTERN_ROOT/apps/bdb_rep/
rm out -rf
make eval EXE_PREFIX=LD_PRELOAD=$XTERN_ROOT/dync_hook/interpose.so
mv out $path/
mv *.log $path/
