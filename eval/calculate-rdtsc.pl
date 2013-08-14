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
	my $startClock = 0;
	my $endClock;
	my $syncWaitTime = 0.0; # global wait time in sync operations.
	my $globalState = "CLOSE";	# can be OPEN, or CLOSE.
	my $globalSyncDepth = 0;
	my $globalStartClock = 0;


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

		# Calculate global wait time. I use a conservative way, the "projecting" method.
		if ($startClock == 0) {
			$startClock = $clock;
		}
		$endClock = $clock;	
		
		if ($opSuffix eq "START") {
			if ($globalState eq "CLOSE" && $globalSyncDepth == 0) {
				$globalStartClock = $clock;
				$globalState = "OPEN";
				dbg "DEBUG: sync wait time OPEN by $tid $op $opSuffix $clock\n";
			}
			$globalSyncDepth = $globalSyncDepth + 1;
		} else {
			$globalSyncDepth = $globalSyncDepth - 1;
			if ($globalSyncDepth == 0 && $globalState eq "OPEN") {
				$globalState = "CLOSE";
				$delta = $clock - $globalStartClock;
				$syncWaitTime = $syncWaitTime + $delta;
				$globalStartClock = 0;
				dbg "DEBUG: sync wait time CLOSE by $tid $op $opSuffix $clock, delta $delta, sync wait time $syncWaitTime\n\n";
			}
		}

		# Calculate per-sync operation wait time.
		
	}
	close(LOG);

	# Print stat.
	print "\n\n\n";
	print "CPU frequency $CPUFREQ MHz.\n";
	print "Global execution clock $totalClock (".($endClock - $startClock)/$CPUFREQ*0.000001." s).\n";
	print "Global sync wait clock $syncWaitTime (".$syncWaitTime/$CPUFREQ*0.000001." s).\n";
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

		system("sort -t \" \" -k4 $file > sorted-$file");
		processSortedFile("$file");
	}
}

chdir($ARGV[0]);
parseLog($ARGV[0]);
chdir($curDir);

