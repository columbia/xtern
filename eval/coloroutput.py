#!/usr/bin/env python
# encoding: utf-8
# ref: http://stackoverflow.com/questions/384076/how-can-i-color-python-logging-output
import logging

def add_coloring_to_emit_ansi(fn):
    # add methods we need to the class
    def new(*args):
        levelno = args[1].levelno
        if(levelno>=50):
            color = '\x1b[1;37;41m' # bright white with red background
        elif(levelno>=40):
            color = '\x1b[31m' # red
        elif(levelno>=30):
            color = '\x1b[33m' # yellow
        elif(levelno>=20):
            color = '\x1b[1;37;40m' # bright white
        elif(levelno>=10):
            color = '\x1b[1;30m' # bright black
        else:
            color = '\x1b[0m' # normal
        args[1].msg = color + args[1].msg +  '\x1b[0m'  # normal
        #print "after"
        return fn(*args)
    return new

import platform
if platform.system()!='Windows':
    logging.StreamHandler.emit = add_coloring_to_emit_ansi(logging.StreamHandler.emit)
    #log = logging.getLogger()
    #log.addFilter(log_filter())
