from libc.stdint cimport uint8_t

from libcpp cimport bool
from libcpp.deque cimport deque
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string
from libcpp.vector import vector

from boink.hashing cimport *
from boink.dbg cimport *

__PlaceHolder__ = ValueError

cdef extern from "boink/assembly.hh" namespace "boink":
    ctypedef deque[char] Path

    cdef cppclass _AssemblerMixin "boink::AssemblerMixin" [GraphType]:
        
        ctypedef shared_ptr[GraphType] GraphPtr

        _AssemblerMixin(GraphPtr)

        void clear_seen()

        uint8_t degree_left()
        uint8_t degree_right()
        uint8_t degree()

        bool get_left(shift_t&)
        bool get_right(shift_t&)
        bool check_neighbors(vector[shift_t],
                             shift_t&)

        void assemble_left(const string&, Path&)
        void assemble_left(Path&)

        void assemble_right(const string&, Path&)
        void assemble_right(Path&)

        void assemble(const string&, Path&)
        void assemble(Path&)

        string to_string(Path&)

        # HashShifter methods
        hash_t set_cursor(string&)
        string get_cursor()
        void get_cursor(deque[char]&)

        bool is_valid(const char)
        bool is_valid(const string&)

        hash_t get()
        hash_t hash(string&)

        vector[shift_t] gather_left()
        vector[shift_t] gather_right()

        hash_t shift_left(const char)
        hash_t shift_right(const char)

    _AssemblerMixin[GraphType] make_assembler[GraphType](shared_ptr[GraphType])


include "assembly_types.pxd.pxi"