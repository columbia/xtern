#!/usr/bin/python3
import os
import subprocess
import shutil

# Check whether APPS_DIR exists
try:
    app_dir = os.environ['APPS_DIR']                    #store $APPR_DIR
except KeyError:
    print("Please set $APPS_DIR")
    exit()

app_path = app_dir + "/apache/"                         #store $APPR_DIR/apache
httpd_path = app_path + "/apache-install/bin/httpd"     #apache path
cwd = os.getcwd()                                       #current dir
apache_version = "2.2.22"                               #version of apache webserver

# Check whetehr XTERN_ROOT exists
try:
    xtern_root = os.environ['XTERN_ROOT']
except KeyError:
    print("Please set $XTERN_ROOT")
    exit()

#ld_preload = "LD_PRELOAD=/home/yihlin/xtern/xtern/dync_hook/interpose.so"
ld_preload = "LD_PRELOAD=" + xtern_root + "/dync_hook/interpose.so"

os.system("ps -C httpd -o pid=|xargs kill -9 >/dev/null 2>&1");

os.chdir(app_path)
print("change directory to " + app_path)
# try to find "httpd"
if not os.path.exists(httpd_path):
    subprocess.Popen("./mk-httpd " + apache_version, shell=True).communicate()
shutil.copy("apache-install/bin/ab", cwd)
# see the --help of setup-httpd for other options, i.e. port number
subprocess.call("./setup-httpd", shell=True)

print("check httpd OK\nchange directory back")
os.chdir(cwd)
start_httpd_path = app_path + '/start-httpd'
command = ld_preload +' '+ start_httpd_path + ' ' +httpd_path + ' -f ' + app_path + '/apache-install/conf/httpd.conf'
#command = start_httpd_path + ' ' +httpd_path + ' -f ' + app_path + '/apache-install/conf/httpd.conf'
print(command)
subprocess.Popen(command, shell=True)
    
