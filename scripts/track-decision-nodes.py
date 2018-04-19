#!/usr/bin/env python

import argparse
import sys

from boink.args import build_dBG_args
from boink.dbg import make_dBG
from boink.cdbg import StreamingCompactor
from boink.processors import DecisionNodeProcessor

def parse_args():
    parser = build_dBG_args()
    parser.add_argument('-o', dest='output_filename', default='/dev/stdout')
    parser.add_argument('-i', dest='inputs', nargs='+', default=['/dev/stdin'])
    parser.add_argument('--output-interval', type=int, default=10000)

    args = parser.parse_args()
    return args


def main():
    args = parse_args()

    graph = make_dBG(args.ksize,
                     args.max_tablesize,
                     args.n_tables)
    compactor = StreamingCompactor(graph)
    processor = DecisionNodeProcessor(compactor,
                                      args.output_filename,
                                      args.output_interval)

    for filename in args.inputs:
        processor.process(filename)

    if args.savegraph:
        graph.save(args.savegraph)


if __name__ == '__main__':
    main()
