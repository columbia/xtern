#!/usr/bin/perl -w

# This script parses the input options file passed in as $ARGV[0] and
# generates an $options.h in directory $ARGV[1] and and $options.cpp file
# in directory $ARGV[2].  The input options file is a collection of "key =
# value" lines.  For each option key, this script creates a variable
# options::key.  User calls read_options(options_file) to set these
# variables to the value specified in options_file.

eval 'exec perl -w -S $0 ${1+"$@"}'
    if 0;

use strict;
use File::Path qw(mkpath);

my $default_opt_file = shift @ARGV;
my $hfile_dir        = shift @ARGV;
my $cppfile_dir      = shift @ARGV;

my $note = 
    "// DO NOT EDIT -- automatically generated by $0\n".
    "// from $default_opt_file\n";
my %options = ();

sub main {

    mkpath($hfile_dir);
    mkpath($cppfile_dir);

    my $hfile = "$hfile_dir/options.h";
    my $cppfile = "$cppfile_dir/options.cpp";

    read_optf($default_opt_file, \%options);
    emit_if_diff(\%options, $hfile, \&emit_header);
    emit_if_diff(\%options, $cppfile, \&emit_cppfile);
}

sub read_optf($$)
{
    my ($file, $optref) = @_;
    return unless -f $file;

    open OPTF, $file || die $!;
    while (<OPTF>) {
	next if /^#/ || /^\s*$/; # skip comments

        # check for simple typo
	if(/^([^\s]+)\s*$/) {
	    warn "warning: No value specified for option $1::$2 at line $. in $file!\n";
	    exit(1);
	}
        if (/^([^\s=]+)\s+([^\s=]+)\s*$/) {
	    warn "warning: missing = between  $1 and $2 at line $. in $file!\n";
	    exit(1);
	}
        if (!/^([^\s]+)\s*=\s*([^\s]+)\s*$/) {
            warn "mal-formated option at line $. in $file: $_";
            exit(1);
        }
        my ($key, $val) = ($1, $2);
        $val =~ s/^\"(.*)\"$/$1/; # strip quotes
	$optref->{$key} = $val;
    }
    close OPTF;
}

sub emit_if_diff($$$)
{
    my ($optref, $file, $emit_fn) = @_;
    my $tmp = $file.".tmp";

    &$emit_fn ($optref, $tmp);

    if(!-f $file || `diff $file $tmp`) {
	system ("mv $tmp $file");
    } else {
	unlink "$tmp";
    }    
}

sub emit_options($$)
{
    my ($optref, $file) = @_;
    open OPT, ">$file" || die $!;
    print OPT join("\n",
                   map {"_ = $optref->{$_};"}
                   sort keys %$optref), "\n";
    close OPT;
}

sub emit_header($$)
{
    my ($optref, $file) = @_;

    my $def = "__OPTIONS_H";

    # variable declarations
    my $opt_decl = "";
    $opt_decl .= join("\n",
                      map {my $type = opt_type ($optref->{$_});
                           "extern $type ${_};";}
                      sort keys %$optref);
    $opt_decl .= "\n";

    open HEADER, ">$file" || die $!;
    print HEADER<<CODE;
$note

#ifndef $def
#define $def

#include <string>

namespace options {

$opt_decl

void read_options(const char *f);
void print_options(void);
void print_options(const char *f);

}

#endif

CODE

    close HEADER;
}

sub emit_cppfile($$)
{
    my ($optref, $file) = @_;

    # read_option_inter function body
    my $read_option_body = "";
    $read_option_body .=
        join("\n",
             map {
                 my $type = opt_type ($optref->{$_});
                 my $res = "  if (key == \"$_\")\n";
                 if($type eq "std::string") {
                     $res .= "    { options::${_} = val; return 1; }";
                 } elsif ($type eq "float") {
                     $res .= "    { options::${_} = (float)atof(val.c_str()); return 1; }";
                 } else {
                     $res .= "    { options::${_} = (int)strtoul(val.c_str(), 0, 0); return 1; }";
                 }
                 $res;} sort keys %$optref);

    # print_options_to_stream function body
    my $print_options_body = "";
    $print_options_body .= 
        join("\n",
             map {"  o << \"${_} = \" << options::${_} << endl;";}
             sort keys %$optref);

    # print variable definitions
    my $opt_def = "";
    $opt_def .=
        join("\n",
             map {my $type = opt_type ($optref->{$_});
                  if ($type eq "std::string") {
                      "$type ${_} = \"$optref->{$_}\";\n";
                  } else {
                      "$type ${_} = $optref->{$_};\n";
                  }} sort keys %$optref);
    $opt_def .= "\n";

    open CFILE, ">$file" || die $!;
    print CFILE<<CODE;

$note
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <assert.h>

#include "tern/options.h"

using namespace std;

namespace options {

$opt_def

static int read_option_inter (string &key, string &val);
static void print_options_to_stream (ostream &o);
static int parse_next_option(ifstream& f, string& key, string& val);

void read_options(const char *f)
{
  ifstream fs(f);
  if (!fs) {
    cerr << "Unable to open " << f << endl;
    return;
  }

  string key, val;
  while (parse_next_option (fs, key, val))
    read_option_inter(key, val);
}

void print_options (void)
{
  print_options_to_stream (cout);
}

void print_options (const char *f)
{
  ofstream fs(f);
  if (!f) {
    cerr << "Unable to open " << f << endl;
    return;
  }
  print_options_to_stream (fs);
  fs << "\\n#DO NOT DELETE\\n";
}

static void print_options_to_stream (ostream &o)
{
$print_options_body
}

static int parse_next_option(ifstream& f, string& key, string& val)
{
  string line;
  while(!f.eof()){
    getline(f, line);
    if(line.size() > 0 && line[0] != '#')
      break; // found a nonempty noncommented line
    //found a comment line; skip it
    f.ignore(65536, '\\n'); //breaks if > 65536 chars in a line
  }
  if(f.eof()) return 0;

  f >> val;
  if(!f.eof())
    f.ignore(65536, '\\n'); //breaks if > 65536 chars in a line

  string::size_type sep;
  sep = line.find('=');
  if(sep == string::npos){
    cerr << "Separator '=' not found in " << line << endl;
    assert(0 && \"invalid option\");
  }
  key = line.substr(0, sep);
  key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());

  val = line.substr(sep+1);
  val.erase(remove_if(val.begin(), val.end(), ::isspace), val.end());
  return 1;
}

static int read_option_inter (string &key, string &val)
{
$read_option_body
  return 0;
}

}

CODE

    close CFILE;
}

sub opt_type($)
{
    my ($value) = @_;
    if ($value =~ /^[+-]?\d+$/) {return "int";}
    elsif($value =~ /^0x[\da-fA-F]+$/) { return "unsigned";}
    #elsif ($value =~ /^[+-]?\d*\.\d+$/) {return "float";}
    else {return "std::string";}
}

main;
