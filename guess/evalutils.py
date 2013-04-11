#!/usr/bin/env python
# encoding: utf-8

import os

def mkDirP(path):
    try:
        os.makedirs(path)
    except OSError as exc: # for newer python version; can specify flags
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            logging.warning("%s already exists" % path)
            pass
        else: raise

def checkExist(file, flags=os.X_OK):
    if not os.path.exists(file) or not os.path.isfile(file) or not os.access(file, flags):
        return False
    return True

def copyFile(src, dst):
    import shutil
    shutil.copy(src, dst)

# ref: twisted-12.3.0 procutils.py
def which(name, flags=os.X_OK):
    result = []
    path = os.environ.get('PATH', None)
    if path is None:
        return []
    for p in os.environ.get('PATH', '').split(os.pathsep):
        p = os.path.join(p, name)
        if os.access(p, flags):
            result.append(p)
    return result

