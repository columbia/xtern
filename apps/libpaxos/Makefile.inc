# Dir containing libdb.a and db.h
BDB_DIR		= $(APPS_DIR)/bdb/install/lib
# Dir containing libevent.a and event.h
LEV_DIR		= $(PWD)/../lib
# Turns on/off verbosity of make
QUIET		= @


LIBPAXOS	= libpaxos.a
MODULES 	= lib test
AR			= ar
ARFLAGS		= rcs
CC			= gcc
MAKE		= make --no-print-directory

SNAME		:= $(shell uname)

CFLAGS 		= -O3 -g -Wall -Wshadow -Wextra 
# Other useful flags:  -DNDEBUG -g

ifeq ($(strip $(SNAME)),Linux)
LDFLAGS		= ../libpaxos.a $(LEV_DIR)/libevent.a $(BDB_DIR)/libdb.a -lpthread -lrt
else
LDFLAGS		= ../libpaxos.a $(LEV_DIR)/libevent.a -lpthread
endif

CPPFLAGS	= -I../include/ -I../ -I$(BDB_DIR) -I$(LEV_DIR)

%.o: %.c
	@echo "> Object $@ <"
	$(QUIET) test -d .deps || mkdir .deps;
	$(QUIET) $(CC) $(CPPFLAGS) $(CFLAGS) $< -MT $@ -MM -MP -MF .deps/$*.d
	$(QUIET) $(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	
%: %.c
	@echo "> Target $@ <"
	$(QUIET) $(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@ 

.PHONY: clean
