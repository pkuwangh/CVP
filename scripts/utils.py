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
    lock,
    trace: str,
    binary: str,
    config: str,
    outfile: Optional[str] = None
) -> None:
    print(f'running {trace}')
    root_path = get_root_path(__file__, 1)
    cvp_bin = joinpath(root_path, 'cvp_ref' if binary == 'ref' else 'cvp')
    cmd = [cvp_bin]
    if config.startswith('vp'):
        cmd.append('-v')
        level = int(config[2])
        cmd.extend(['-t', str(level)])
    cmd.append(trace)
    proc = launch_proc(cmd, outfile)
    proc.wait()
    if outfile:
        with lock:
            print(f'Output written to {os.path.relpath(outfile)}')
            with open(outfile, 'rt') as fp:
                for line in fp.readlines():
                    if line.lstrip().startswith('IPC'):
                        print(line.rstrip())
    else:
        print(proc.stdout.decode('utf-8'))
