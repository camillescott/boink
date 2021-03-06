# goetia/tests/test_processors.py
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

import pytest

import csv

from .utils import *
from goetia.dbg import dBG
import screed

def test_dbg_inserter(graph, datadir, ksize):
    consumer = type(graph).Processor.build(graph, 10000, 10000, 10000)
    rfile = datadir('random-20-a.fa')

    n_reads = consumer.process(rfile)

    graph2 = graph.shallow_clone()
    for record in screed.open(rfile):
        for kmer in kmers(record.sequence, ksize):
            assert graph.get(kmer)
            assert not graph2.get(kmer)
            assert graph.get(kmer) != graph2.get(kmer)

    for record in screed.open(rfile):
        graph2.insert_sequence(record.sequence)
        for kmer in kmers(record.sequence, ksize):
            assert graph.get(kmer) == graph2.get(kmer)


def test_chunked_dbg_inserter(graph, datadir, ksize):
    consumer = type(graph).Processor.build(graph, 10000, 10000, 10000)
    rfile = datadir('random-20-a.fa')

    for _ in consumer.chunked_process(rfile):
        pass

    graph2 = graph.shallow_clone()
    for record in screed.open(rfile):
        for kmer in kmers(record.sequence, ksize):
            assert graph.get(kmer)
            assert not graph2.get(kmer)
            assert graph.get(kmer) != graph2.get(kmer)

    for record in screed.open(rfile):
        graph2.insert_sequence(record.sequence)
        for kmer in kmers(record.sequence, ksize):
            assert graph.get(kmer) == graph2.get(kmer)
