#!/usr/bin/env python3

import os
import subprocess
from os.path import join as joinpath
from typing import List, Optional


def get_root_path(ref_file: str, level: int):
    root_path = os.path.dirname(os.path.abspath(ref_file))
    for _ in range(level):
        root_path = os.path.dirname(root_path)
    return root_path


def launch_proc(
    cmd: List[str],
    filename: Optional[str] = None,
) -> subprocess.Popen:
    if filename:
        with open(filename, 'wt') as fp:
            fp.write(' '.join(cmd))
            return subprocess.Popen(
                cmd,
                stdout=fp,
                stderr=fp,
            )
    else:
        return subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )


def runner(
    trace: str,
    config: str,
    outfile: Optional[str] = None
) -> None:
    print(f'running {trace}')
    root_path = get_root_path(__file__, 1)
    cvp_bin = joinpath(root_path, 'cvp')
    cmd = [cvp_bin]
    if config.startswith('vp'):
        cmd.append('-v')
        level = int(config[2])
        cmd.extend(['-t', str(level)])
    cmd.append(trace)
    proc = launch_proc(cmd, outfile)
    proc.wait()
    if outfile:
        print(f'Output written to {outfile}')
    else:
        print(proc.stdout.decode('utf-8'))
