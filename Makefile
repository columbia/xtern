#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL := .

DIRS := common recorder analyzer replayer tools
EXTRA_DIST := 

ifeq ($(MAKECMDGOALS),unittests)
  DIRS += unittests
  OPTIONAL_DIRS :=
endif

#
# Include the Master Makefile that knows how to build all.
#
include $(LEVEL)/Makefile.common

.PHONY: cscope.files
cscope.files:
	find \
          common recorder analyzer replayer tests \
          -name Makefile -or \
          -name \*.in -or \
          -name \*.c -or \
          -name \*.cpp -or \
          -name \*.exp -or \
          -name \*.inc -or \
          -name \*.h | sort > cscope.files
