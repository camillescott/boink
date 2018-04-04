# boink/dbg.pxd.pxi
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.
#
# WARNING: this file is automatically generated; do not modify it!
# The source template is: dbg.tpl.pxd

from libcpp.memory cimport shared_ptr, make_shared
from boink.assembly cimport _AssemblerMixin

cdef class dBG_Base:
    cdef readonly object storage_type
    cdef readonly object shifter_type
    cdef readonly object suffix
    cdef object allocated


cdef class dBG_BitStorage_DefaultShifter(dBG_Base):
    cdef shared_ptr[_dBG[BitStorage,DefaultShifter]] _this
    cdef shared_ptr[_AssemblerMixin[_dBG[BitStorage,DefaultShifter]]] _assembler
    cdef hash_t _handle_kmer(self, object) except 0

cdef class dBG_NibbleStorage_DefaultShifter(dBG_Base):
    cdef shared_ptr[_dBG[NibbleStorage,DefaultShifter]] _this
    cdef shared_ptr[_AssemblerMixin[_dBG[NibbleStorage,DefaultShifter]]] _assembler
    cdef hash_t _handle_kmer(self, object) except 0

cdef class dBG_ByteStorage_DefaultShifter(dBG_Base):
    cdef shared_ptr[_dBG[ByteStorage,DefaultShifter]] _this
    cdef shared_ptr[_AssemblerMixin[_dBG[ByteStorage,DefaultShifter]]] _assembler
    cdef hash_t _handle_kmer(self, object) except 0


cdef object _make_dbg(int K, uint64_t starting_size, int n_tables, 
                     str storage=*, str shifter=*)

