import os
import re
import sys
import subprocess
import logging
import signal
import eval

def model_checking(configs, benchmark):
    try:
        from lxml import etree
        #logging.debug("running with lxml.etree")
    except ImportError:
        try:
            # Python 2.5
            import xml.etree.cElementTree as etree
            #logging.debug("running with cElementTree on Python 2.5+")
        except ImportError:
            try:
                # Python 2.5
                import xml.etree.ElementTree as etree
                #logging.debug("running with ElementTree on Python 2.5+")
            except ImportError:
                try:
                    # normal cElementTree install
                    import cElementTree as etree
                    #logging.debug("running with cElementTree")
                except ImportError:
                    try:
                        # normal ElementTree install
                        import elementtree.ElementTree as etree
                        #logging.debug("running with ElementTree")
                    except ImportError:
                        logging.error("Failed to import ElementTree from any known place") 

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
    command = ' '.join([export, exec_file] + inputs.split())
    program.set("command", command)

    with open("run.xml", "w") as run_xml:
        run_xml.write(etree.tostring(dbug_config, pretty_print=True))

    init_env_cmd = configs.get(benchmark, "INIT_ENV_CMD")
    if init_env_cmd:
        os.system(init_env_cmd)

    # copy files
    eval.copy_file(os.path.realpath(exec_file), os.path.basename(exec_file))

    bash_path = eval.which('bash')[0]
    dbug_cmd = '%s run.xml' % EXPLORER
    with open('dbug.log', 'w', 102400) as log_file:
        #logging.info("executing '%s'" % dbug_cmd)
        proc = subprocess.Popen(dbug_cmd, stdout=log_file, stderr=subprocess.STDOUT,
                                shell=True, executable=bash_path, bufsize = 102400, preexec_fn=os.setsid)
        proc.wait()
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except OSError:
            pass
    
