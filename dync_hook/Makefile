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

#
# Relative path to the top of the source tree.
#
LEVEL := ..
include $(LEVEL)/Makefile.common

INC_DIRS=-I$(SRC_ROOT)/include/ -I$(OBJ_ROOT)/include -I.
STD_LIBS=-lsupc++ -lpthread -lstdc++ -lrt
TERN_LIBS=-L$(OBJ_ROOT)/$(BuildMode)/lib/ -lruntime -lcommon
CFLAGS += $(INC_DIRS)
CXXFLAGS += $(INC_DIRS)
SRC_DIR=$(LEVEL)/../dync_hook
ifndef $(XTERN_PLUS_DBUG)
	XTERN_PLUS_DBUG=0
endif

all:: interpose.so libxtern-annot.so interpose_mc.so

interpose.so: template.cpp spec_hooks.cpp hook.cpp
ifneq ($(XTERN_PLUS_DBUG),1)
	gcc -fPIC -rdynamic $(SRC_DIR)/hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose.so interpose.o -o interpose.so -ldl $(TERN_LIBS) $(STD_LIBS)
endif

interpose_mc.so: template.cpp spec_hooks.cpp hook.cpp
ifeq ($(XTERN_PLUS_DBUG),1)
	gcc -fPIC -rdynamic $(SRC_DIR)/hook.cpp $(CFLAGS) -c -o interpose.o
	gcc $(CFLAGS) -shared -Wl,-soname,interpose_mc.so interpose.o -o interpose_mc.so -ldl $(TERN_LIBS) $(STD_LIBS) \
	-Wl,--rpath,$(SMT_MC_ROOT)/mc-tools/dbug/install/lib -L$(SMT_MC_ROOT)/mc-tools/dbug/install/lib -ldbug-stubs
endif

libxtern-annot.so: xtern-annot.cpp
	gcc -fPIC -rdynamic $(SRC_DIR)/xtern-annot.cpp $(CFLAGS) -c -o xtern-annot.o
	gcc $(CFLAGS) -shared -Wl,-soname,libxtern-annot.so xtern-annot.o -o libxtern-annot.so -ldl $(TERN_LIBS) $(STD_LIBS)

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $

code_gen: code_gen.cpp
	g++ $^ $(CXXFLAGS) -o code_gen -ldl

template.cpp: code_gen func_template.cpp void_func_template.cpp hook_func.def
	./code_gen $(SRC_DIR)/func_template.cpp $(SRC_DIR)/void_func_template.cpp < $(SRC_DIR)/hook_func.def

clean::
	rm -rf *.o
	rm -rf *.so
	rm -rf code_gen template.cpp
	rm -rf $(SRC_DIR)/*.o
	rm -rf $(SRC_DIR)/*.so
	rm -rf $(SRC_DIR)/code_gen $(SRC_DIR)/template.cpp

install::
	cp *.so $(SRC_DIR)

