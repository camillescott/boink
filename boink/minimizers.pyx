# boink/minimizers.pyx
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

from cython.operator cimport dereference as deref

from libcpp.memory cimport make_unique

from boink.utils cimport _bstring

cdef class InteriorMinimizer:

    def __cinit__(self, int64_t window_size, *args, **kwargs):
        if type(self) is InteriorMinimizer:
            self._im_this = make_unique[_InteriorMinimizer[hash_t]](window_size)
            self._this = self._im_this.get()

    @property
    def window_size(self):
        return deref(self._this).window_size()

    def reset(self):
        deref(self._this).reset()

    def update(self, hash_t value):
        '''Update the minimizer with a new value. Returns 
        tuple of (current_min, index).'''

        cdef pair[hash_t, int64_t] result = deref(self._this).update(value)
        return result.first, result.second

    def __call__(self, object values):
        for value in values:
            self.update(value)
        return self.get_minimizer_values()

    def get_minimizers(self):
        cdef vector[pair[hash_t, int64_t]] minimizers = deref(self._this).get_minimizers()
        return minimizers

    def get_minimizer_values(self):
        cdef vector[hash_t] values = deref(self._this).get_minimizer_values()
        return values


cdef class WKMinimizer(InteriorMinimizer):
    
    def __cinit__(self, int64_t window_size, uint16_t ksize, *args, **kwargs):
        if type(self) is WKMinimizer:
            self._wk_this = make_unique[_WKMinimizer[DefaultShifter]](window_size, ksize)
            self._this = <_InteriorMinimizer[hash_t]*>self._wk_this.get()

    def get_minimizers(self, str sequence):
        cdef string _sequence = _bstring(sequence)
        return deref(self._wk_this).get_minimizers(_sequence)

    def get_minimizer_kmers(self, str sequence):
        cdef string _sequence = _bstring(sequence)
        return deref(self._wk_this).get_minimizer_kmers(_sequence)