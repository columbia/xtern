#!/bin/sh

if [ $# -ne 1 ] 
then
    echo "./compile.sh filename (ignore .cpp)"
    exit
fi

file=$1.cpp
output=$1
 
if [ -f $file ]
then
    echo "Compiling..."
else
    echo "Current directory doesn't contain $1.cpp"
    exit
fi

#file=rand-shuf.cpp
#output=rand-shuf

#file=sort.cpp
#output=sort

#file=set_union.cpp
#output=set_union

# test native gcc
# g++ -fopenmp -lgomp -D_GLIBCXX_PARALLEL file

XTERN_ANNOT_LIB="-I$XTERN_ROOT/include -L$XTERN_ROOT/dync_hook -Wl,--rpath,$XTERN_ROOT/dync_hook -lxtern-annot"
$XTERN_ROOT/apps/openmp/install/bin/g++ -g -static-libgcc -static-libstdc++ -D_GLIBCXX_PARALLEL -I./util -L. -fopenmp $file -o $output -lgomp $XTERN_ANNOT_LIB -fno-stack-protector
