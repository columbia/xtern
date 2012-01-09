/* -*- Mode: C++ -*- */

/*
 *
 * Copyright (C) 2008 Junfeng Yang (junfeng@cs.columbia.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <cstdlib>
#include <assert.h>

#include "tern/options.h"

#    define xi_panic(fmt, arg...) \
            abort()
#    define forit(it, v) \
	    for((it) = (v).begin(); (it) != (v).end(); ++(it))

using namespace std;

static list<struct options*> *_options;

static int load_option_inter (const char* dom, 
                               const char* opt, const char* val);
static void print_options_to_stream (ostream &o);
static int parse_next_option(ifstream& f, string& dom,
                             string& opt, string& val);

register_options::register_options(struct options *o)
{
    if(_options == NULL)
        _options = new list<struct options*>;
    _options->push_back(o);
}

void init_options()
{
    static int initp = 0; 
    if(initp)
        return;
    initp = 1;

    list<struct options *>::iterator i;
    forit(i, *_options)
        (*i)->init();
}

void load_options(const char *f)
{
    init_options();

    ifstream fs(f);
    if (!fs) {
	cerr << "Unable to open " << f << endl;
	return;
    }

    string dom, opt, val;
    int numInstalled = 0;

    while (parse_next_option (fs, dom, opt, val))
        numInstalled += 
            load_option_inter(dom.c_str(), opt.c_str(), val.c_str());
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
    fs << "\n#DO NOT DELETE\n";
}

void clean_options (void)
{
    list<struct options *>::iterator i;
    forit(i, *_options)
        delete *i;
}

static int parse_next_option(ifstream& f, string& dom,
                             string& opt, string& val)
{

  string domOption;
  while(!f.eof()){
    f >> domOption;
    if(domOption.size() > 0 && domOption[0] != '#')
      break; // found a nonempty noncommented line
    //found a comment and so skip the whole line
    f.ignore(65536, '\n'); //breaks if > 65536 chars in a line
  }
  if(f.eof()) return 0;
  f >> val;
  if(!f.eof())
    f.ignore(65536, '\n'); //breaks if > 65536 chars in a line


  string::size_type sep = domOption.find("::");
  if(sep == string::npos){
    cerr << "Domain separator :: not found in " << domOption << endl;
    int invalid_option = 0;
    assert(invalid_option);
  }
  dom = domOption.substr(0,sep);
  opt = domOption.substr(sep+2, string::npos);

  return 1;
}

static int load_option_inter (const char* dom, 
                              const char* opt, const char* val)
{
    list<struct options*>::iterator i;
    forit(i, *_options) {
        if((*i)->load(dom, opt, val)) {
            //Heming comments this.
            //printf("loaded %s, %s, %s\n", dom, opt, val);
            return 1;
        }
    }
    xi_panic("unknown option: %s::%s %s", dom, opt, val);
}

static void print_options_to_stream (ostream &o)
{
    list<struct options*>::iterator i;
    forit(i, *_options)
        (*i)->print(o);
}
