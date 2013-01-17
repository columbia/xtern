#!/bin/bash

cd $XTERN_ROOT/llvm
rm -rf llvm-* install
wget http://llvm.org/releases/2.7/llvm-2.7.tgz
wget http://llvm.org/releases/2.7/llvm-gcc4.2-2.7-x86_64-linux.tar.bz2
tar zxvf llvm-2.7.tgz
tar xJvf llvm-gcc4.2-2.7-x86_64-linux.tar.bz2

if [ -z $1 ]; then
  ./scripts/mk-llvm
else
  ./scripts/mk-llvm $1
fi
