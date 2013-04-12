#!/usr/bin/env python
import coloroutput
import logging
import argparse
import sys
import numpy as np
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt

def readPerf(file):
    name = {}
    perf = {}
    try:
        for line in open(file, 'r').readlines():
            line = line.partition('#')[0].rstrip()
            if not line:
                continue
            eval_id = int(line.split()[0])
            name[eval_id] = line.split()[1]
            perf[eval_id] = float(line.split()[2])
    except IOError as e:
        logging.error(str(e))
        sys.exit(1)
    return name, perf

def mergeDicts(*dicts):
    from itertools import chain
    return dict(chain(*[d.iteritems() for d in dicts]))

if __name__ == "__main__":
    # set log format
    logger = logging.getLogger()
    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s",
                                  "%Y%b%d-%H:%M:%S")
    ch = logging.StreamHandler()
    ch.setFormatter(formatter)
    ch.setLevel(logging.DEBUG)
    logger.addHandler(ch)
    logger.setLevel(logging.DEBUG)

    # parse input arguments
    parser = argparse.ArgumentParser(
        description="Draw perforamnce chart of Parrot benchmarks")
    parser.add_argument('perfs', nargs='*',
        type=str,
        help = "list of performance reports")
    args = parser.parse_args()
    
    # read base
    nondet_name, nondet_perf = readPerf('base_nondet')
    parrot_name, parrot_perf = readPerf('base_parrot')
    parrot_hint_name, parrot_hint_perf = readPerf('base_parrot_w_hint')
    if args.perfs:
        input_name, input_perf = readPerf(args.perfs[0])
    if args.perfs:
        name_list = input_name
    else:
        name_list = mergeDicts(nondet_name, parrot_name, parrot_hint_name)
    
    n_t_parrot_dict = {}
    n_t_parrot_hint_dict = {}
    n_t_input_dict = {}
    for k in name_list:
        n_t_parrot_dict[k] = parrot_perf[k]/nondet_perf[k]
        n_t_parrot_hint_dict[k] = parrot_hint_perf[k]/nondet_perf[k]
        if args.perfs:
            n_t_input_dict[k] = input_perf[k]/nondet_perf[k]
    

    N = len(name_list)
    ind = np.arange(N)
    width = 0.3

    n_t_parrot = tuple(n_t_parrot_dict[key] for key in n_t_parrot_dict)
    n_t_parrot_hint = tuple(n_t_parrot_hint_dict[key] for key in n_t_parrot_hint_dict)
    if args.perfs:
        n_t_input = tuple(n_t_input_dict[key] for key in n_t_parrot_dict)

    fig = plt.figure()
    ax = fig.add_subplot(1, 1, 1)

    rects = ax.bar(ind, n_t_parrot, width, color='r')
    rects = ax.bar(ind+width, n_t_parrot_hint, width, color='y')
    if args.perfs:
        rects = ax.bar(ind+width*2, n_t_input, width, color='w')

    ax.set_xticks(ind+0.5)
    ax.set_xticklabels( list(name_list[key] for key in name_list), rotation=90)

    plt.tight_layout() 
    plt.show()

