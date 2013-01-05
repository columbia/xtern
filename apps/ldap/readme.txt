0. a good link to setup openldap.
http://blog.csdn.net/magic881213/article/details/7848577

1. Build.
cd $XTERN_ROOT/apps/ldap
./mk

2. Start server (server is started as debug mode, and listens to 4008 port).
./install/libexec/slapd.x86 -d256 -f $XTERN_ROOT/apps/ldap/install/etc/openldap/slapd.conf -h ldap://:4008

3. Add one entry (just a simple client operation).
./install/bin/ldapadd -H ldap://:4008 -x -D "cn=manager, dc=example,dc=com" -w secret -f test2.ldif

4. Run a concurrent benchmark with the openldap server. The benchmark comes from the test suite of openldap.
./run
