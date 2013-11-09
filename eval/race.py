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

import os
import stat
import re
import sys
import subprocess
import logging
import signal
import time
import eval

def race_detection(configs,benchmark,args,inputs):
# get environment variables
	try:
		SMT_MC_ROOT = os.environ["SMT_MC_ROOT"]
	except KeyError as e:
		logging.error("Please set the environment variable " + str(e))
		sys.exit(1)
	XTERN_ROOT = os.environ["XTERN_ROOT"]
	XTERN_PRELOAD = "LD_PRELOAD=%s/dync_hook/interpose.so" % XTERN_ROOT
	DATA_RACE_DETECTION_ROOT = os.environ["DATA_RACE_DETECTION_ROOT"]
	APPS = os.path.abspath("%s/apps" % XTERN_ROOT)
# check self-built valgrind
	apps_name, exec_file = eval.extract_apps_exec(benchmark, APPS)
	VALGRIND = "%s/thread-sanitizer/install/bin/valgrind" % DATA_RACE_DETECTION_ROOT
	if not eval.checkExist(VALGRIND):
		logging.error("%s does not exist" % VALGRIND)
		sys.exit(1)
# get 'export' environment variable
	export = configs.get(benchmark, 'EXPORT')
	if export:
		logging.debug("export %s", export)

# git environment presetting command
	init_env_cmd = configs.get(benchmark, 'INIT_ENV_CMD')
	if init_env_cmd:
		logging.info("presetting cmd in each round %s" % init_env_cmd)
# generate command for xtern
	xtern_command = ' '.join([XTERN_PRELOAD,export,exec_file] + inputs.split())
	nondet_command = ' '.join([export,exec_file] + inputs.split())
	logging.info("executing command is %s" % xtern_command)
# generate run.sh
	with open("run-det.sh",'w') as runsh_file:
		runsh_file.write(xtern_command)
		st=os.stat("run-det.sh")
		os.chmod("run-det.sh",st.st_mode|stat.S_IEXEC)
	with open("run-nondet.sh",'w') as runsh_file:
		runsh_file.write(nondet_command)
		st=os.stat("run-nondet.sh")
		os.chmod("run-nondet.sh",st.st_mode|stat.S_IEXEC)
	init_env_cmd = configs.get(benchmark, "INIT_ENV_CMD")
	if init_env_cmd:
		os.system(init_env_cmd)
# copy files 
	eval.copy_file(os.path.realpath(exec_file), os.path.basename(exec_file))
	bash_path = eval.which('bash')[0]
# helgrind
	if args.race_detection == "helgrind":
		LOG_FILENAME="helgrind.log"
		rd_nondet_cmd = ' '.join([VALGRIND,"--gen-suppressions=yes","--trace-children=yes","--read-var-info=yes","--log-file="+LOG_FILENAME,"--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/libc.supp","--tool=helgrind","./run-nondet.sh"])
		PARROT_LOG_FILENAME="parrot-helgrind.log"
		rd_det_cmd = ' '.join([VALGRIND,"--gen-suppressions=yes","--trace-children=yes","--read-var-info=yes","--log-file="+PARROT_LOG_FILENAME,"--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/libc.supp","--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/parrot-helgrind.supp","--tool=helgrind","./run-det.sh"])
# tsan
	if args.race_detection == "tsan":
		LOG_FILENAME="tsan.log"
		rd_nondet_cmd = ' '.join([VALGRIND,"--gen-suppressions=yes","--trace-children=yes","--read-var-info=yes","--log-file="+LOG_FILENAME,"--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/libc.supp","--tool=tsan","./run-nondet.sh"])
		PARROT_LOG_FILENAME="parrot-tsan.log"
		rd_det_cmd = ' '.join([VALGRIND,"--gen-suppressions=yes","--trace-children=yes","--read-var-info=yes","--log-file="+PARROT_LOG_FILENAME,"--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/libc.supp","--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/parrot-tsan.supp","--tool=tsan","./run-det.sh"])
# tsan-hybrid
	if args.race_detection == "tsan-hybrid":
		LOG_FILENAME="tsan-hybrid.log"
		rd_nondet_cmd = ' '.join([VALGRIND,"--gen-suppressions=yes","--trace-children=yes","--read-var-info=yes","--log-file="+LOG_FILENAME,"--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/libc.supp","--tool=tsan","--hybrid=yes","./run-nondet.sh"])
		PARROT_LOG_FILENAME="parrot-tsan-hybrid.log"
		rd_det_cmd = ' '.join([VALGRIND,"--gen-suppressions=yes","--trace-children=yes","--read-var-info=yes","--log-file="+PARROT_LOG_FILENAME,"--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/libc.supp","--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/parrot-tsan.supp","--suppressions="+DATA_RACE_DETECTION_ROOT+"/thread-sanitizer/parrot-tsan-hybrid.supp","--tool=tsan","--hybrid=yes","./run-det.sh"])
# executing
	FNULL = open(os.devnull, 'w')
	logging.info(rd_nondet_cmd)
	proc=subprocess.Popen(rd_nondet_cmd,shell=True,executable=bash_path,bufsize=102400,stdout=FNULL,stderr=FNULL)
	proc.wait()
	if args.race_detection in ["tsan","tsan-hybrid"]:
		proc=subprocess.Popen("grep 'ThreadSanitizer summary' "+LOG_FILENAME+" |sed '1d'",shell=True)
	if args.race_detection in ["helgrind"]:
		proc=subprocess.Popen("grep 'ERROR SUMMARY' "+LOG_FILENAME+" |sed '1d'",shell=True)
	proc.wait()
	logging.info(rd_det_cmd)
	proc=subprocess.Popen(rd_det_cmd,shell=True,executable=bash_path,bufsize=102400,stdout=FNULL,stderr=FNULL)
	proc.wait()
	if args.race_detection in ["tsan","tsan-hybrid"]:
		proc=subprocess.Popen("grep 'ThreadSanitizer summary' "+PARROT_LOG_FILENAME+" |sed '1d'",shell=True)
	if args.race_detection in ["helgrind"]:
		proc=subprocess.Popen("grep 'ERROR SUMMARY' "+PARROT_LOG_FILENAME+" |sed '1d'",shell=True)
	proc.wait()
