# boink/compactor.pxd
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

cimport cython

from libcpp.deque cimport deque
from libcpp.set cimport set
from libcpp.vector cimport vector
from libc.stdint cimport uint8_t, uint32_t, uint64_t

from boink.assembly cimport *
from boink.hashing cimport *
from boink.dbg cimport *
from boink.cdbg cimport *
from boink.minimizers cimport _InteriorMinimizer
from boink.events cimport (_StreamingCompactorReport, _EventNotifier,
                           _EventListener, EventNotifier, EventListener)


cdef extern from "boink/compactor.hh" namespace "boink" nogil:

    cdef struct _compact_segment "boink::compact_segment":
        hash_t left_anchor
        hash_t right_anchor
        hash_t left_flank
        hash_t right_flank
        size_t start_pos
        size_t length
        bool is_decision_kmer

        compact_segment()
        const bool is_null() 

    cdef cppclass _StreamingCompactor "boink::StreamingCompactor" [GraphType] (_AssemblerMixin[GraphType], _EventNotifier):
        _cDBG * cdbg

        _StreamingCompactor(GraphType *)

        #string compactify(const string&) except +ValueError
        #void compactify_right(Path&) 
        #void compactify_left(Path&)

        void wait_on_updates()

        void find_decision_kmers(const string&,
                                 vector[uint32_t]&,
                                 vector[hash_t]&,
                                 vector[NeighborBundle]&) except +ValueError

        void update_sequence(const string&) except +ValueError

        void find_new_segments(const string&, # sequence to add
                               deque[_compact_segment]&, # new segments
                               ) except +ValueError

        _StreamingCompactorReport* get_report()




include "compactor.tpl.pxd.pxi"