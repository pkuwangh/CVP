#!/usr/bin/env python3

import argparse
import json
import os

from utils import get_root_path


def main(args):
    root_path = get_root_path(__file__, 1)
    dst_path = os.path.join(root_path, 'traces')
    link_file = os.path.join(dst_path, 'links.txt')
    links = {'public': [], 'secret': []}
    with open(link_file, 'rt') as fp:
        for line in fp.readlines():
            link = line.rstrip()
            if 'public_traces' in link:
                links['public'].append(link)
            else:
                links['secret'].append(link)
    # got all info
    if args.list:
        print(json.dumps(links, indent=4))
    tags = []
    if args.public:
        tags.append('public')
    if args.secret:
        tags.append('secret')
    for tag in tags:
        for link in links[tag]:
            cmd = f'wget {link}'
            if args.fwdproxy:
                cmd = 'http_proxy=fwdproxy:8080 ' + cmd
            try:
                os.system(cmd)
            except Exception as e:
                print(f'When downloading [{link}], got {str(e)}')
            else:
                print(f'Done [{link}]')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--list', '-l', action='store_true',
        help='list traces')
    parser.add_argument('--fwdproxy', '-f', action='store_true',
        help='use fwdproxy')
    parser.add_argument('--public', '-p', action='store_true',
        help='download small public traces')
    parser.add_argument('--secret', '-s', action='store_true',
        help='download large secret traces')
    args = parser.parse_args()
    main(args)
