#!/bin/bash

MACHINE=`uname -m`
cd $XTERN_ROOT/llvm
rm -rf llvm-* install
wget http://llvm.org/releases/2.7/llvm-2.7.tgz

if [ $MACHINE == "x86_64" ]; then
  wget http://llvm.org/releases/2.7/llvm-gcc4.2-2.7-$MACHINE-linux.tar.bz2
  tar jxf llvm-gcc4.2-2.7-$MACHINE-linux.tar.bz2
else
  wget http://llvm.org/releases/2.7/llvm-gcc-4.2-2.7-$MACHINE-linux.tgz
  tar zxf llvm-gcc-4.2-2.7-$MACHINE-linux.tgz
  mv llvm-gcc-4.2-2.7-$MACHINE-linux llvm-gcc4.2-2.7-$MACHINE-linux
fi

#exit;

tar zxf llvm-2.7.tgz

if [ -z $1 ]; then
  ./scripts/mk-llvm
else
  ./scripts/mk-llvm $1
fi
