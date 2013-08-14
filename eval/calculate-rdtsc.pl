#!/usr/bin/perl

# Usage: ./calculate-rdtsc.pl <directory which stores a recorded schedule from one execution>
# E.g., ./draw-time-chart.pl ./rdtsc_out

$numThreads = 0;
%totalEvents;

$CPUFREQ = `lscpu | grep "MHz" | awk \'{print \$3}\'`;
chomp($CPUFREQ);
$curDir = `pwd`;
unless(-d @ARGV[0]){
    print "Directory @ARGV[0] for rdtsc does not exist, this is fine, bye...";
}

$debug = 1;
sub dbg {
	if ($debug) {
		print $_[0];
	}
}

sub processSortedFile {
	my $sortedFile = $_[0];
	my @fields;
	my $tid;
	my $op;
	my $opSuffix;
	my $clock;
	my $delta;

	# global states.
	my $numTid = 0;
	my %tidMap;								# map from pthread self id to the showing up tid order.
	my %startClocks; 								# key is tid+op
	my %endClocks; 								# key is tid+op
	my $globalSyncTime = 0;
	my %tidClocks;								# key is tid
	my %syncClocks;								# key is op
	my $startClock = 0;
	my $endClock = 0;


	# Open the file and process it.
	open(LOG, $sortedFile);
	foreach $line (<LOG>) {
		@fields = split(/ /, $line);
		$tid = $fields[0];
		$op = $fields[1];
		$opSuffix = $fields[2];
		$clock = $fields[3];
		chomp($clock);
		#print "PRINT $tid $op $opSuffix $clock\n";

		# update start and end clock.
		if ($startClock == 0) {
			$startClock = $clock;
		}
		$endClock = $clock;

		if ($opSuffix eq "START") {
			$startClocks{$tid.$key} = $clock;
			if ($tidMap{$tid} eq "") {
				$tidMap{$tid} = $numTid;
				$numTid++;
			}
		} else {
			$endClocks{$tid.$key} = $clock;
			# And update stats here.
			$delta = $endClocks{$tid.$key} - $startClocks{$tid.$key};
			if ($delta > 1e6) {
				dbg "$tid $op delta: (end: $endClocks{$tid.$key}, start: $startClocks{$tid.$key}) $delta.\n";
			}

			# Update global stat.
			$globalSyncTime += $delta;

			# Update per thread stat.
			if ($tidClocks{$tid} eq "") {
				#dbg "New thread $tid.\n";
				$tidClocks{$tid} = $delta;
			} else {
				#dbg "Old thread $tid.\n";
				$tidClocks{$tid} += $delta;
			}

			# Update per sync op stat.
			if ($syncClocks{$op} eq "") {
				$syncClocks{$op} = $delta;
			} else {
				$syncClocks{$op} += $delta;
			}

		}

		
	}
	close(LOG);

	# Print stat.
	print "\n\n\n";
	print "CPU frequency $CPUFREQ MHz.\n";
	print "Global execution clock ".($endClock - $startClock)." (".($endClock - $startClock)/$CPUFREQ*0.000001." s).\n";

	# Per tid.
	print "\nSorted by sync wait time, ascending:\n";
	print "Global sync wait clock $globalSyncTime of all threads (".$globalSyncTime/$CPUFREQ*0.000001." s).\n";
	for $key ( sort {$tidClocks{$a} <=> $tidClocks{$b}} keys %tidClocks) {
		print "Thread $tidMap{$key} (pthread self $key) sync wait time $tidClocks{$key} (".$tidClocks{$key}/$CPUFREQ*0.000001." s).\n";
	}
	print "\n";

	# Per sync.
	print "\nSorted by sync wait time, ascending:\n";
	for $key ( sort {$syncClocks{$a} <=> $syncClocks{$b}} keys %syncClocks) {
		print "Sync $key sync wait time $syncClocks{$key} (".$syncClocks{$key}/$CPUFREQ*0.000001." s).\n";
	}
	print "\n";

	print "\n\n\n";
}

sub parseLog {
	my $dirPath = $_[0];

	opendir (DIR, ".");
	while (my $file = readdir(DIR)) {
		next if ($file =~ m/^\./);
		next if ($file =~ m/\.log/);
		next if ($file =~ m/^sorted/);
		print "Processing $dirPath/$file...\n";

		#system("sort -t \" \" -k4 $file > sorted-$file");
		processSortedFile("$file");
	}
}

chdir($ARGV[0]);
parseLog($ARGV[0]);
chdir($curDir);

