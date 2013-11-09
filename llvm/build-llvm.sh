#!/bin/bash

#
# Copyright (c) 2013,  Regents of the Columbia University 
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other 
# materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

MACHINE=`uname -m`
cd $XTERN_ROOT/llvm
rm -rf llvm-* install
wget http://llvm.org/releases/2.7/llvm-2.7.tgz

if [ $MACHINE == "x86_64" ]; then
  wget http://llvm.org/releases/2.7/llvm-gcc4.2-2.7-$MACHINE-linux.tar.bz2
  tar xjvf llvm-gcc4.2-2.7-$MACHINE-linux.tar.bz2
else
  wget http://llvm.org/releases/2.7/llvm-gcc-4.2-2.7-$MACHINE-linux.tgz
  tar xzvf llvm-gcc-4.2-2.7-$MACHINE-linux.tgz
  mv llvm-gcc-4.2-2.7-$MACHINE-linux llvm-gcc4.2-2.7-$MACHINE-linux
fi

#exit;

tar zxvf llvm-2.7.tgz

if [ -z $1 ]; then
  ./scripts/mk-llvm
else
  ./scripts/mk-llvm $1
fi
