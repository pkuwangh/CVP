#!/usr/bin/env python3

import argparse
import json
import multiprocessing
import os
from concurrent.futures import ProcessPoolExecutor
from datetime import datetime
from os.path import join as joinpath
from typing import List

from utils import get_root_path, runner


def info(args) -> None:
    if args.suites:
        trace_suites = {}
        root_path = joinpath(get_root_path(__file__, 1), 'traces')
        for x1 in os.listdir(root_path):
            if os.path.isdir(joinpath(root_path, x1)):
                trace_suites[x1] = []
                for x2 in os.listdir(joinpath(root_path, x1)):
                    trace_suites[x1].append(x2)
        print(json.dumps(trace_suites, indent=4))


def get_trace_list(args) -> List[str]:
    if len(args.tracefile) > 0:
        return list(args.tracefile)
    elif args.tracedir:
        trace_list = []
        for root, _, files in os.walk(args.tracedir):
            for x in sorted(files):
                if x.endswith('.gz'):
                    trace_list.append(joinpath(root, x))
        return trace_list
    else:
        print('Use -t or -d to specify traces')
        exit(-1)


def get_outfile(args, tracefile: str):
    root_path = get_root_path(__file__, 1)
    tracefile_name = os.path.basename(tracefile)
    suite_name = os.path.basename(get_root_path(tracefile, 1))
    outfile_name = (
        suite_name + '_' +
        tracefile_name.replace('.gz', f'_{args.config}_{args.binary}.txt')
    )
    outfile = os.path.join(root_path, 'results', outfile_name)
    return outfile


def run(args) -> None:
    with ProcessPoolExecutor(max_workers=args.num_workers) as executor:
        lock = multiprocessing.Manager().RLock()
        futures = []
        results = []
        traces = get_trace_list(args)
        for infile in traces:
            outfile = get_outfile(args, infile)
            futures.append(executor.submit(
                runner, lock, infile, args.binary, args.config, outfile))
        while len(results) < len(futures):
            for idx, f in enumerate(futures):
                if f and f.done():
                    results.append(f.result())
                    print('[{:s}] finished {:4d} / {:d} jobs'.format(
                        datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                        len(results), len(traces),
                    ))
                    futures[idx] = None
        if len(args.tracefile) > 0:
            for line in sorted(results):
                print(line)
        else:
            summary_file = f'summary_{args.config}_{args.binary}.txt'
            with open(summary_file, 'wt') as fp:
                for line in sorted(results):
                    fp.write(line + '\n')
            print(f'summary written to {summary_file}')


def init_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    # sub-command parsers
    sub_parsers = parser.add_subparsers(help='Commands')
    info_parser = sub_parsers.add_parser('info',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        help='show trace info')
    run_parser = sub_parsers.add_parser('run',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        help='run test on a trace')
    # info configs
    info_parser.add_argument('--suites', '-s', action='store_true',
        help='list trace suites')
    # run configs
    run_parser.add_argument('--num-workers', '-n', type=int, default=12,
        help='number of parallel workers')
    run_parser.add_argument('--binary', '-b', type=str, required=True,
        choices=['ref', 'my',],
        help='binary version')
    run_parser.add_argument('--config', '-c', type=str, required=True,
        choices=['base', 'vp0_all', 'vp1_load', 'vp2_miss',],
        help='run config')
    run_parser.add_argument('--tracefile', '-t', action='append', default=[],
        help='trace file to run')
    run_parser.add_argument('--tracedir', '-d', type=str, default=None,
        help='trace directory')
    # commands
    info_parser.set_defaults(func=info)
    run_parser.set_defaults(func=run)
    return parser


if __name__ == '__main__':
    parser = init_parser()
    args = parser.parse_args()
    args.func(args)
