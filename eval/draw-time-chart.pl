#!/usr/bin/perl

# Usage: ./draw-time-chart.pl <directory which stores a recorded schedule from one execution>
# E.g., ./draw-time-chart.pl ./out

$numThreads = 0;
%totalEvents;
$curDir;

sub parseSchedule {
	my $dirPath = $_[0];
	my $tid;
	my $op;
	my $instrId;
	my $turn;
	my $appTime = 0;
	my $syscallTime = 0;
	my $schedTime = 0;
	my @fields1;
	my @fields2;
	my %tids;
	
	chdir($dirPath);
	opendir (DIR, ".");
	while (my $file = readdir(DIR)) {
		next if ($file =~ m/^\./);
		print "$file\n";

		@fields1 = split(/-/, $file);
		@fields2 = split(/\./, $fields1[2]);
		$tid = $fields2[0];

		# Open the file and process it.
		open(LOG, $file);
		foreach $line (<LOG>) {
			@fields1 = split(/ /, $line);
			$op = $fields1[0];
			next if ($op =~ "tern_idle"); # Skip turn idle for now.
			$turn = $fields1[2];
			$appTime = $fields1[3];
			$syscallTime = $fields1[4];
			$schedTime = $fields1[5];

			$appTime =~ s/:/\./g;
			$syscallTime =~ s/:/\./g;
			$schedTime =~ s/:/\./g;
			
			print "$turn:$tid:$appTime\n";
			$totalEvents{$turn} = $tid.":".$op;
		}
		close(LOG);
	}
}

sub drawTimeChart {
	# TBD.
	open(CHART, ">"."../time-chart.txt");
	foreach $key (sort {$totalEvents{$a} <=> $totalEvents{$b} } keys %totalEvents) {
		print CHART $key." --> ".$totalEvents{$key}."\n";
	}
	close(CHART);
}

$curDir = `pwd`;
parseSchedule(@ARGV[0]);
print $curDir."\n";
chdir($curDir);
drawTimeChart();

