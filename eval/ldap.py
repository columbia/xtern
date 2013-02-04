import os
import re
import sys
import subprocess
import eval

def killall(process_name):
    import subprocess, signal
    p = subprocess.Popen(['ps', '-A'], stdout=subprocess.PIPE)
    out, err = p.communicate()

    for line in out.splitlines():
        if process_name in line:
            pid = int(line.split(None, 1)[0])
            os.kill(pid, signal.SIGKILL)

def evaluation(repeats = 100):
    root = os.getcwd()
    XTERN_ROOT = os.environ["XTERN_ROOT"]
    bash_path = eval.which('bash')[0]

    # relink local.options
    local_options = '%s/apps/ldap/openldap-2.4.33/obj/tests/local.options' % XTERN_ROOT
    try:
        os.unlink(local_options)
    except OSError:
        pass
    os.symlink('%s/local.options' % root, local_options)

    # xtern part
    cmd = ' '.join(['PRELOAD_LIB=%s/dync_hook/interpose.so' % XTERN_ROOT, 'make', 'test'])
    out_dir = '%s/xtern' % root
    eval.mkdir_p(out_dir)
    for i in range(repeats):
        killall('slapd')
        sys.stderr.write("\tPROGRESS: %5d/%d\r" % (i+1, int(repeats))) # progress
        with open('%s/output.%d' % (out_dir, i), 'w', 102400) as log_file:
            os.chdir('%s/apps/ldap/openldap-2.4.33/obj' % XTERN_ROOT)
            proc = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT, shell=True, executable=bash_path, bufsize = 102400, preexec_fn=os.setsid)
            proc.wait()
            os.chdir(root)
        # move log files into 'xtern' directory
        os.renames('%s/apps/ldap/openldap-2.4.33/obj/tests/out-bdb' % XTERN_ROOT, '%s/out.%d' % (out_dir, i))

    # non-det part
    cmd = ' '.join(['PRELOAD_LIB=%s/eval/rand-intercept/rand-intercept.so' % XTERN_ROOT, 'make', 'test'])
    out_dir = '%s/non-det' % root
    eval.mkdir_p(out_dir)
    for i in range(repeats):
        killall('slapd')
        sys.stderr.write("\tPROGRESS: %5d/%d\r" % (i+1, int(repeats))) # progress
        with open('%s/output.%d' % (out_dir, i), 'w', 102400) as log_file:
            os.chdir('%s/apps/ldap/openldap-2.4.33/obj' % XTERN_ROOT)
            proc = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT, shell=True, executable=bash_path, bufsize = 102400, preexec_fn=os.setsid)
            proc.wait()
            os.chdir(root)
        # move log files into 'xtern' directory
        os.renames('%s/apps/ldap/openldap-2.4.33/obj/tests/out-bdb' % XTERN_ROOT, '%s/out.%d' % (out_dir, i))

    # restore
    os.unlink(local_options)
    os.symlink('%s/apps/ldap/local.options' % XTERN_ROOT, local_options)

    # get stats
    xtern_cost = []
    for i in range(int(repeats)):
        log_file_name = 'xtern/output.%d' % i
        for line in reversed(open(log_file_name, 'r').readlines()):
            if re.search('^real [0-9]+\.[0-9][0-9]$', line):
                xtern_cost += [float(line.split()[1])]
                break

    nondet_cost = []
    for i in range(int(repeats)):
        log_file_name = 'non-det/output.%d' % i
        for line in reversed(open(log_file_name, 'r').readlines()):
            if re.search('^real [0-9]+\.[0-9][0-9]$', line):
                nondet_cost += [float(line.split()[1])]
                break

    eval.write_stats(xtern_cost, nondet_cost, int(repeats))
