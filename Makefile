#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL := .

DIRS := lib dync_hook
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
	cd $(PROJ_SRC_ROOT) && find \
          include lib tools unittests test \
          -name Makefile -or \
          -name \*.in -or \
          -name \*.c -or \
          -name \*.cpp -or \
          -name \*.exp -or \
          -name \*.inc -or \
          -name \*.h | sort > cscope.files && \
	cscope -b
