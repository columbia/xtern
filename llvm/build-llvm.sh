#!/bin/bash

cd $XTERN_ROOT/llvm
rm -rf llvm-* install
wget http://llvm.org/releases/2.7/llvm-2.7.tgz
wget http://llvm.org/releases/2.7/llvm-gcc-4.2-2.7.source.tgz
tar zxvf llvm-2.7.tgz
tar zxvf llvm-gcc-4.2-2.7.source.tgz

if [ -z $1 ]; then
  ./scripts/mk-llvm
else
  ./scripts/mk-llvm $1
fi
