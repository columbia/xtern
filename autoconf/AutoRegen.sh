#!/bin/sh

<<<<<<< HEAD
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


if ([ "$#" != 1 ] || 
    [ ! -d "$1" ] ||
    [ ! -d "$1/autoconf/m4" ]); then
    echo "usage: $0 <llvmsrc-dir>" 1>& 2
    exit 1
fi

llvm_src_root=$(cd $1; pwd)
llvm_m4=$llvm_src_root/autoconf/m4
die () {
	echo "$@" 1>&2
	exit 1
}

test -d autoconf && test -f autoconf/configure.ac && cd autoconf
test -f configure.ac || die "Can't find 'autoconf' dir; please cd into it first"
# Patch LLVM_SRC_ROOT in configure.ac
sed -e "s#^LLVM_SRC_ROOT=.*#LLVM_SRC_ROOT=\"$llvm_src_root\"#" \
    configure.ac > configure.tmp.ac
echo "Regenerating aclocal.m4 with aclocal"
rm -f aclocal.m4
echo aclocal -I $llvm_m4 -I "$llvm_m4/.." || die "aclocal failed"
aclocal -I $llvm_m4 -I "$llvm_m4/.." || die "aclocal failed"
echo "Regenerating configure with autoconf"
echo autoconf --warnings=all -o ../configure configure.tmp.ac || die "autoconf failed"
autoconf --warnings=all -o ../configure configure.tmp.ac || die "autoconf failed"
cp ../configure ../configure.bak
sed -e "s#^LLVM_SRC_ROOT=.*#LLVM_SRC_ROOT=\".\"#" \
    ../configure.bak > ../configure
cd ..
echo "Regenerating config.h.in with autoheader"
autoheader --warnings=all \
    -I autoconf -I autoconf/m4 \
    autoconf/configure.tmp.ac || die "autoheader failed"
rm -f autoconf/configure.tmp.ac configure.bak
exit 0
