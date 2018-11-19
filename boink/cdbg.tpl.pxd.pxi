# boink/cdbg.tpl.pxd.pxi
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.
#
# WARNING: this file is automatically generated; do not modify it!
# The source template is: cdbg.tpl.pxd

from libcpp.memory cimport shared_ptr

cdef class cDBG_Base:
    cdef readonly object shifter_type
    cdef readonly object storage_type

cdef class cDBG__BitStorage__DefaultShifter(cDBG_Base):
    cdef shared_ptr[_cDBG[_dBG[_BitStorage,_DefaultShifter]]] _this
    cdef public EventNotifier Notifier

    @staticmethod
    cdef cDBG__BitStorage__DefaultShifter _wrap(shared_ptr[_cDBG[_dBG[_BitStorage,_DefaultShifter]]])
cdef class cDBG__ByteStorage__DefaultShifter(cDBG_Base):
    cdef shared_ptr[_cDBG[_dBG[_ByteStorage,_DefaultShifter]]] _this
    cdef public EventNotifier Notifier

    @staticmethod
    cdef cDBG__ByteStorage__DefaultShifter _wrap(shared_ptr[_cDBG[_dBG[_ByteStorage,_DefaultShifter]]])
cdef class cDBG__NibbleStorage__DefaultShifter(cDBG_Base):
    cdef shared_ptr[_cDBG[_dBG[_NibbleStorage,_DefaultShifter]]] _this
    cdef public EventNotifier Notifier

    @staticmethod
    cdef cDBG__NibbleStorage__DefaultShifter _wrap(shared_ptr[_cDBG[_dBG[_NibbleStorage,_DefaultShifter]]])
cdef class cDBG__SparseppSetStorage__DefaultShifter(cDBG_Base):
    cdef shared_ptr[_cDBG[_dBG[_SparseppSetStorage,_DefaultShifter]]] _this
    cdef public EventNotifier Notifier

    @staticmethod
    cdef cDBG__SparseppSetStorage__DefaultShifter _wrap(shared_ptr[_cDBG[_dBG[_SparseppSetStorage,_DefaultShifter]]])

