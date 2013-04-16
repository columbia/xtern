LEVEL := ../..

LIBRARYNAME := runtime
BUILD_ARCHIVE := 1

include $(LEVEL)/Makefile.common

OPTIMIZE_OPTION = -O3

CFLAGS += -g -fPIC -rdynamic
CXXFLAGS += -g -fPIC -rdynamic
LDFLAGS += -g -fPIC -rdynamic

ifeq ($(XTERN_PLUS_DBUG),1)
  CXXFLAGS += -I$(SMT_MC_ROOT)/mc-tools/dbug/include -L$(SMT_MC_ROOT)/mc-tools/dbug/install/lib -DXTERN_PLUS_DBUG  
endif
