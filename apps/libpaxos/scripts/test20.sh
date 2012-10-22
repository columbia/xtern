#!/bin/bash

#config_file="./config_slow.cfg"
config_file="./test20.cfg"
acceptor_bin="../acceptor"
learner_bin="../client_learner"
client_bin="../client_proposer"

######################################################################
acceptor_offset=10;
learner_offset=10;
client_offset=800;

learner_count=0;
client_count=0;

function bin_check () {
	for f in "$learner_bin" "$acceptor_bin" "$client_bin"; do
	if [[ ! -x $f ]]; then
		echo "File not found $f"
		exit 1;
	fi
done	
}

function launch_acceptor () {
	local acc_id=$1
	local launchcmd="$acceptor_bin $acc_id $config_file"
    if [ $acc_id -eq 1 ]; then
    #if true; then
    #if false; then
	    echo "LD_PRELOAD=$XTERN_ROOT/dync_hook/interpose.so $launchcmd"
        (LD_PRELOAD=$XTERN_ROOT/dync_hook/interpose.so $launchcmd &> accept$acc_id.log) &
    else
        echo "$launchcmd"
        (expect_unbuffer $launchcmd &> accept$acc_id.log) &
    fi
	sleep 1;
}

function launch_learner () {
    let "learner_count += 1";
	local launchcmd="$learner_bin $config_file"
	echo "$launchcmd"
	expect_unbuffer $launchcmd 1>&2 > learner$learner_count.log &
	sleep 1;
}

function launch_client () {
    let "client_count += 1";
	local launchcmd="$client_bin $config_file "
	echo "$launchcmd"
	expect_unbuffer $launchcmd 1>&2 > client$client_count.log &
	sleep 1;
}

######################################################################

killall acceptor > /dev/null
killall client_learner > /dev/null
killall client_proposer > /dev/null

bin_check

launch_learner

launch_learner

acceptors=`cat $config_file | grep ^acceptor | sort -r | awk '{print $2}' | xargs`
for n in $acceptors; do
	echo "Starting acceptor $n"
	launch_acceptor $n
done

launch_client
wait

echo "All processes terminated"

