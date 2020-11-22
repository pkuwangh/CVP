#!/usr/bin/env python3

import argparse
import json
import os
from concurrent.futures import ProcessPoolExecutor
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
            trace_list.extend([joinpath(root, x) for x in sorted(files)])
        return trace_list
    else:
        print('Use -t or -d to specify traces')
        exit(-1)


def run(args) -> None:
    root_path = get_root_path(__file__, 1)
    with ProcessPoolExecutor(max_workers=12) as executor:
        for infile in get_trace_list(args):
            tracefile_name = os.path.basename(infile)
            suite_name = os.path.basename(get_root_path(infile, 1))
            outfile_name = suite_name + '_' + tracefile_name.replace('.gz', '.txt')
            outfile = os.path.join(root_path, 'results', outfile_name)
            executor.submit(runner, infile, args.config, outfile)


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
