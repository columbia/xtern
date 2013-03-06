import os
import re
import sys
import subprocess
import logging
import signal
import time
import eval

class DbugTimeout(Exception):
    pass

def DbugTimeoutHandler(signum, frame):
    raise DbugTimeout

def model_checking(configs, benchmark, args):
    try:
        from lxml import etree
    except ImportError:
        logging.error("Please install 'lxml' e.g. sudo easy_install-2.7 lxml")
        sys.exit(1)

    # get environment variables
    try:
        SMT_MC_ROOT = os.environ["SMT_MC_ROOT"]
    except KeyError as e:
        logging.error("Please set the environment variable " + str(e))
        sys.exit(1)
    XTERN_ROOT = os.environ["XTERN_ROOT"]
    APPS = os.path.abspath("%s/apps" % XTERN_ROOT)

    apps_name, exec_file = eval.extract_apps_exec(benchmark, APPS)

    # check dbug
    ARBITER = "%s/mc-tools/dbug/install/bin/dbug-arbiter" % SMT_MC_ROOT
    if not eval.checkExist(ARBITER):
        logging.error("%s does not exist" % ARBITOR)
    EXPLORER = "%s/mc-tools/dbug/install/bin/dbug-explorer" % SMT_MC_ROOT
    if not eval.checkExist(EXPLORER):
        logging.error("%s does not exist" % EXPLORER)

    # generate run.xml
    arbiter_port = configs.get(benchmark, 'DBUG_ARBITER_PORT')
    explorer_port = configs.get(benchmark, 'DBUG_EXPLORER_PORT')
    dbug_timeout = configs.get(benchmark, 'DBUG_TIMEOUT')
    program_input = configs.get(benchmark, 'DBUG_INPUT')
    program_output = configs.get(benchmark, 'DBUG_OUTPUT')

    dbug_config = etree.Element("config")
    arbiter = etree.SubElement(dbug_config, "arbiter")
    explorer = etree.SubElement(dbug_config, "explorer")
    interposition = etree.SubElement(dbug_config, "interposition")
    program = etree.SubElement(dbug_config, "program")

    arbiter.set("port", arbiter_port)
    arbiter.set("command", "%s -l" % ARBITER)
    explorer.set("log_dir", ".")
    explorer.set("port", explorer_port)
    explorer.set("timeout", dbug_timeout)
    interposition.set("path", "%s/mc-tools/dbug/install/lib/libdbug.so" % SMT_MC_ROOT)
    inputs = configs.get(benchmark, "INPUTS")
    export = configs.get(benchmark, "EXPORT")
    command = ' '.join([exec_file] + inputs.split())
    program.set("command", command)
    if program_input:
        program.set("input", program_input)
    if program_output:
        program.set("output", program_output)

    with open("run.xml", "w") as run_xml:
        run_xml.write(etree.tostring(dbug_config, pretty_print=True))

    # generate run_xtern.xml
    interposition.set("path", "%s/xtern/dync_hook/interpose_mc.so" % SMT_MC_ROOT)

    with open("run_xtern.xml", "w") as run_xtern_xml:
        run_xtern_xml.write(etree.tostring(dbug_config, pretty_print=True))

    init_env_cmd = configs.get(benchmark, "INIT_ENV_CMD")
    if init_env_cmd:
        os.system(init_env_cmd)

    # copy files
    eval.copy_file(os.path.realpath(exec_file), os.path.basename(exec_file))

    bash_path = eval.which('bash')[0]
    dbug_cmd = '%s %s run.xml' % (export, EXPLORER)
    dbug_xtern_cmd = '%s %s run_xtern.xml' % (export, EXPLORER)

    if args.generate_xml_only:
        logging.info(dbug_cmd);
        logging.info(dbug_xtern_cmd);
        return

    local_timeout = int(float(dbug_timeout)*1.1)
    if not args.smtmc_only:
        with open('dbug.log', 'w', 102400) as log_file:
            signal.signal(signal.SIGALRM, DbugTimeoutHandler)
            signal.alarm(local_timeout)
            try:
                logging.info(dbug_cmd)
                proc = subprocess.Popen(dbug_cmd, stdout=log_file, stderr=subprocess.STDOUT,
                                    shell=True, executable=bash_path, bufsize = 102400, preexec_fn=os.setsid)
                proc.wait()
                signal.alarm(0)
            except DbugTimeout:
                signal.alarm(0)
                logging.warning("'%s' with dbug does not stop after %d seconds, kill it..." % (benchmark, local_timeout))
            except KeyboardInterrupt:
                pass
            try:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.kill()
            except OSError:
                pass
        time.sleep(1)
    
    if not args.dbug_only:
        with open('dbug_xtern.log', 'w', 102400) as log_file:
            signal.signal(signal.SIGALRM, DbugTimeoutHandler)
            signal.alarm(local_timeout)
            try:
                logging.info(dbug_xtern_cmd)
                proc = subprocess.Popen(dbug_xtern_cmd, stdout=log_file, stderr=subprocess.STDOUT,
                                    shell=True, executable=bash_path, bufsize = 102400, preexec_fn=os.setsid)
                proc.wait()
                signal.alarm(0)
            except DbugTimeout:
                signal.alarm(0)
                logging.warning("'%s' with dbug_xtern does not stop after %d seconds, kill it..." % (benchmark, local_timeout))
            except KeyboardInterrupt:
                pass
            try:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.kill()
            except OSError:
                pass
 
