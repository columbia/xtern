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

// stop being C compatible, to simplify things.  The entire code base 
// is in C++ now, except the kernel drivers.  Can always create a C interface
// if need be.

#ifndef __OPTIONS_H
#define __OPTIONS_H

#include <memory>
#include <iostream>

struct options {
    virtual ~options() {}
    virtual void init() = 0;
    virtual int load(const char *dom, const char *opt, const char *val) = 0;
    virtual void print(std::ostream& f) = 0;
};

struct register_options {
    register_options(struct options *);
};

template<typename T1, typename T2>
bool _opt_eq(const T1& t1, const T2& t2)
{ return t1 == t2; }

template<typename T2>
bool _opt_eq(const char* const &t1, const T2& t2)
{return strcasecmp(t1, t2) == 0; }

template<typename T1, typename T2>
void _set_opt(T1& t1, const T2& t2)
{ t1 = t2; }

template<typename T2>
void _set_opt(const char* &t1, const T2& t2)
{
    if(t1) free((void*)t1);
    t1 = strdup(t2);
}

void init_options();
void load_options(const char *f);
void print_options(void);
void print_options(const char *f);
void clean_options(void);

#define option(dom, opt) (__##dom##__##opt)
#define set_option(dom, opt, val) _set_opt(option(dom, opt), val)
#define get_option(dom, opt) option(dom, opt)
#define option_eq(dom, opt, val) (_opt_eq(option(dom, opt), val))
#define option_ne(dom, opt, val) (!_opt_eq(option(dom, opt), val))

#endif /* !__OPTIONS_H */
