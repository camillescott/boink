# boink/tests/test_cdbg.py
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

import itertools
import sys

import pytest
from boink.tests.utils import *

from boink.cdbg import display_segment_list


@pytest.fixture
def compactor(ksize, graph):
    from boink.cdbg import StreamingCompactor
    compactor = StreamingCompactor(graph)
    return compactor


class TestFindNewSegments:

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_fork_core_first(self, ksize, length, graph, compactor, right_fork,
                                   check_fp):
        (core, branch), pos = right_fork()
        check_fp()

        print('INPUTS', core, core[:pos+1], ' ' * (pos + 1) + branch, sep='\n\n')

        core_segments = compactor.find_new_segments(core)
        graph.add_sequence(core)
        branch_segments = compactor.find_new_segments(core[:pos+1] + branch)
        graph.add_sequence(core[:pos+1] + branch)

        # should be NULL - SEG - NULL
        print('Core Segments')
        display_segment_list(core_segments)
        for s in core_segments:
            print('segment:', s.sequence)
        # again NULL - SEG - NULL
        print('Branch Segments')
        display_segment_list(branch_segments)
        for s in branch_segments:
            print('segment:', s.sequence)

        assert branch_segments[0].is_null
        assert len(core_segments) == 3
        assert len(branch_segments) == 3
        assert branch_segments[1].sequence == branch
        assert branch_segments[1].start == pos + 1
        assert branch_segments[1].is_decision_kmer == False
        assert branch_segments[-1].is_null

        assert core_segments[0].is_null
        assert core_segments[1].sequence == core
        assert len(core_segments[1].sequence) == length
        assert core_segments[1].start == 0
        assert core_segments[1].length == length
        assert core_segments[1].is_decision_kmer == False
        assert core_segments[-1].is_null

        for segment in branch_segments:
            if not segment.is_null:
                for kmer in kmers(segment.sequence, ksize):
                    assert graph.left_degree(kmer) < 2
                    assert graph.right_degree(kmer) < 2

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_right_decision_split(self, ksize, graph, compactor, right_fork,
                                  check_fp):
        (core, branch), pos = right_fork()
        check_fp()

        print('INPUTS', core, core[:pos+1], ' ' * (pos + 1) + branch, sep='\n\n')

        branch_segments = compactor.find_new_segments(branch)
        graph.add_sequence(branch)
        core_segments = compactor.find_new_segments(core)
        graph.add_sequence(core)

        print('Branch Segments')
        display_segment_list(branch_segments)
        for s in branch_segments:
            print('segment:' + s.sequence)

        print('Core Segments')
        display_segment_list(core_segments)
        for s in core_segments:
            print('segment:' + s.sequence)

        assert len(branch_segments) == 3
        assert branch_segments[0].is_null
        assert branch_segments[-1].is_null
        branch_segment = branch_segments[1]
        assert branch_segment.sequence == branch
        assert branch_segment.length == len(branch)

        assert len(core_segments) == 5
        assert core_segments[0].is_null
        assert core_segments[1].is_decision_kmer == False
        assert core_segments[2].is_decision_kmer == True
        assert core_segments[3].is_decision_kmer == False
        assert core_segments[-1].is_null

        assert graph.left_degree(core_segments[2].sequence) == 1
        assert graph.right_degree(core_segments[2].sequence) == 2
        assert graph.left_degree(core[pos:pos+ksize]) == 1
        assert graph.right_degree(core[pos:pos+ksize]) == 2

        assert core[:pos+ksize-1] == core_segments[1].sequence
        assert core[pos+1:] == core_segments[3].sequence


    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_merge_no_decisions(self, ksize, length, graph, compactor, linear_path, check_fp):
        sequence = linear_path()
        left = sequence[:length//2]
        right = sequence[length//2:]
        print(left)
        print(right)
        check_fp()

        left_segments = compactor.find_new_segments(left)
        graph.add_sequence(left)
        right_segments = compactor.find_new_segments(right)
        graph.add_sequence(right)

        assert len(left_segments) == 3
        assert left_segments[0].is_null
        assert left_segments[1].sequence == left
        assert left_segments[1].left_anchor == graph.hash(left[:ksize])
        assert left_segments[1].right_anchor == graph.hash(left[-ksize:])
        assert left_segments[-1].is_null

        assert right_segments[0].is_null
        assert len(right_segments) == 3
        assert right_segments[1].sequence == right
        assert right_segments[-1].is_null

        merged = left + right
        merged_new = left[-ksize+1:] + right[:ksize-1]
        assert sum(graph.get_counts(merged_new)) == 0
        merged_segments = compactor.find_new_segments(merged)
        assert len(merged_segments) == 3
        assert merged_segments[0].is_null
        assert merged_segments[-1].is_null
        merged_segment = merged_segments[1]

        assert merged_segment.sequence == merged_new
        assert merged_segment.left_anchor == graph.hash(merged_new[:ksize])
        assert merged_segment.right_anchor == graph.hash(merged_new[-ksize:])


    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_right_decision_on_end(self, ksize, graph, compactor, right_fork,
                                         check_fp):
        (core, branch), pos = right_fork()
        check_fp()

        # pos is start position of decision k-mer
        print('INPUTS', core, core[:pos+1], ' ' * (pos + 1) + branch, sep='\n\n')
        upper = branch
        lower = core[pos+1:]
        test = core[:pos+ksize]

        upper_segments = compactor.find_new_segments(upper)
        graph.add_sequence(upper)
        lower_segments = compactor.find_new_segments(lower)
        graph.add_sequence(lower)

        assert len(upper_segments) == 3
        assert upper_segments[1].sequence == upper
        assert len(lower_segments) == 3
        assert lower_segments[1].sequence == lower

        test_segments = compactor.find_new_segments(test)
        display_segment_list(test_segments)

        assert test_segments[0].is_null
        assert len(test_segments) == 4
        assert test_segments[2].is_decision_kmer
        assert len(test_segments[2].sequence) == ksize
        assert len(test_segments[1].sequence) == pos + ksize - 1
        assert test_segments[1].left_anchor == graph.hash(core[:ksize])
        assert test_segments[1].right_anchor == graph.hash(core[pos-1:pos+ksize-1])
        assert test_segments[-1].is_null


    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_left_decision_on_end(self, ksize, length, graph, compactor, left_fork,
                                        check_fp):
        (core, branch), pos = left_fork()
        check_fp()
        # pos is start position of decision k-mer
        print('INPUTS', core, core[:pos+1], ' ' * (pos + 1) + branch, sep='\n\n')
        upper = branch
        lower = core[:pos+ksize-1]
        test = core[pos:]

        upper_segments = compactor.find_new_segments(upper)
        graph.add_sequence(upper)
        lower_segments = compactor.find_new_segments(lower)
        graph.add_sequence(lower)

        assert len(upper_segments) == 3
        assert upper_segments[1].sequence == upper
        assert len(lower_segments) == 3
        assert lower_segments[1].sequence == lower

        test_segments = compactor.find_new_segments(test)
        display_segment_list(test_segments)

        assert len(test_segments) == 4
        assert test_segments[1].is_decision_kmer
        assert test_segments[1].sequence == test[:ksize]
        assert len(test_segments[1].sequence) == ksize
        assert len(test_segments[2].sequence) == len(test) - 1
        assert test_segments[2].left_anchor == graph.hash(core[pos+1:pos+ksize+1])
        assert test_segments[2].right_anchor == graph.hash(core[-ksize:])


class TestDecisionNodes(object):

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_new_decision_from_fork(self, ksize, length, graph, compactor,
                                          left_fork, check_fp):
        '''New decision node of form (begin)-[D]-[S]-(end)
        '''

        (core, branch), pos = left_fork()
        check_fp()

        upper = branch
        lower = core[:pos+ksize-1]
        test = core[pos:]

        compactor.update_sequence(upper)
        compactor.update_sequence(lower)
        assert compactor.cdbg.n_dnodes == 0

        compactor.update_sequence(test)
        assert compactor.cdbg.n_dnodes == 1
        dnode_hash = graph.hash(test[:ksize])
        print('dnode hash:', dnode_hash)
        assert compactor.cdbg.has_dnode(dnode_hash)

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_left_end_induced_decision_from_fork(self, ksize, length, graph, compactor,
                                             left_fork, check_fp):
        '''Decision node induced by segment end which is also end of sequence
           of form (begin)-[x]-[D]-[S]-(end)
        '''

        (core, branch), pos = left_fork()
        check_fp()

        compactor.update_sequence(core)
        assert compactor.cdbg.n_dnodes == 0

        compactor.update_sequence(branch)
        assert compactor.cdbg.n_dnodes == 1
        dnode_hash = graph.hash(core[pos:pos+ksize])
        print('dnode hash:', dnode_hash)
        assert compactor.cdbg.has_dnode(dnode_hash)

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_right_end_induced_decision_from_fork(self, ksize, length, graph, compactor,
                                                        right_fork, check_fp):
        '''Decision node induced by segment end which is also end of sequence
           of form (begin)-[S]-[D]-[x]-(end)
        '''

        (core, branch), pos = right_fork()
        check_fp()

        print(core, core[:pos+1] + branch, sep='\n')
        print(core[pos:pos+ksize])

        compactor.update_sequence(core)
        assert compactor.cdbg.n_dnodes == 0

        compactor.update_sequence(branch)
        assert compactor.cdbg.n_dnodes == 1
        dnode_hash = graph.hash(core[pos:pos+ksize])
        print('dnode hash:', dnode_hash)
        assert compactor.cdbg.has_dnode(dnode_hash)

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_left_mid_induced_decision_from_fork(self, ksize, length, graph, compactor,
                                                       left_fork, check_fp):
        ''' Decision node is induced by a non-decision segment end,
            with flanking known sequence to its  right

            (begin)-[S]-[D x]-(end)
        '''
        (core, branch), pos = left_fork()
        check_fp()

        branch = branch + core[pos+ksize-1:]
        print(core, branch, sep='\n')
        print(core[pos:pos+ksize])

        compactor.update_sequence(core)
        assert compactor.cdbg.n_dnodes == 0

        compactor.update_sequence(branch)
        assert compactor.cdbg.n_dnodes == 1
        dnode_hash = graph.hash(core[pos:pos+ksize])
        print('dnode hash:', dnode_hash)
        assert compactor.cdbg.has_dnode(dnode_hash)

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_right_mid_induced_decision_from_fork(self, ksize, length, graph, compactor,
                                                        right_fork, check_fp):
        ''' Decision node is induced by a non-decision segment end,
            with flanking known sequence to its  right

            (begin)-[x D]-[S]-(end)
        '''
        (core, branch), pos = right_fork()
        check_fp()

        branch = core[:pos+1] + branch
        print(core, branch, sep='\n')
        print(core[pos:pos+ksize])

        compactor.update_sequence(core)
        assert compactor.cdbg.n_dnodes == 0

        compactor.update_sequence(branch)
        assert compactor.cdbg.n_dnodes == 1
        dnode_hash = graph.hash(core[pos:pos+ksize])
        print('dnode hash:', dnode_hash)
        assert compactor.cdbg.has_dnode(dnode_hash)

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_new_decision_node_segment_flanked(self, ksize, length, graph, compactor,
                                                     left_hairpin, check_fp):
        ''' Test flanked new decision node using a hairpin fixture, of form
            (begin)-[S]-[D]-[S]-[x]-(end) where [D] is the same k-mer as [x]
        '''
        sequence, pos = left_hairpin()
        check_fp()
        print('d-kmer: ', sequence[pos:pos+ksize], graph.hash(sequence[pos:pos+ksize]))
        compactor.update_sequence(sequence)

        assert compactor.cdbg.n_dnodes == 1
        assert compactor.cdbg.has_dnode(graph.hash(sequence[pos:pos+ksize]))


class TestUnitigBuildExtend(object):

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_left_fork_unode_creation(self, ksize, length, graph, compactor,
                                            left_fork, check_fp):
        '''New decision node of form (begin)-[D]-[S]-(end)
        '''

        (core, branch), pos = left_fork()
        upper = branch
        lower = core[:pos+ksize-1]
        test = core[pos:]

        compactor.update_sequence(upper)
        assert compactor.cdbg.n_unodes == 1
        upper_unode = compactor.cdbg.query_unode_end(graph.hash(upper[:ksize]))
        assert upper_unode.sequence == upper

        compactor.update_sequence(lower)
        assert compactor.cdbg.n_dnodes == 0
        assert compactor.cdbg.n_unodes == 2
        lower_unode = compactor.cdbg.query_unode_end(graph.hash(lower[:ksize]))
        assert lower_unode.sequence == lower

        compactor.update_sequence(test)
        assert compactor.cdbg.n_dnodes == 1
        dnode_hash = graph.hash(test[:ksize])
        print('dnode hash:', dnode_hash)
        assert compactor.cdbg.has_dnode(dnode_hash)

        test_unode = compactor.cdbg.query_unode_end(graph.hash(test[1:ksize+1]))
        assert test_unode.sequence == test[1:]

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_extend_right(self, ksize, length, graph, compactor, linear_path, check_fp):
        sequence = linear_path()
        left = sequence[:length//2]

        compactor.update_sequence(left);
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.query_unode_end(graph.hash(left[:ksize])).sequence == left

        compactor.update_sequence(sequence)
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.query_unode_end(graph.hash(left[:ksize])).sequence == sequence

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_extend_left(self, ksize, length, graph, compactor, linear_path, check_fp):
        sequence = linear_path()
        right = sequence[length//2:]

        compactor.update_sequence(right);
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.query_unode_end(graph.hash(right[:ksize])).sequence == right

        compactor.update_sequence(sequence)
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.query_unode_end(graph.hash(sequence[:ksize])).sequence == sequence

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_merge(self, ksize, length, graph, compactor, linear_path, check_fp):
        sequence = linear_path()
        left = sequence[:length//2]
        right = sequence[length//2:]
        print(left)
        print(right)

        compactor.update_sequence(left);
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.query_unode_end(graph.hash(left[:ksize])).sequence == left

        compactor.update_sequence(right)
        assert compactor.cdbg.n_unodes == 2
        assert compactor.cdbg.query_unode_end(graph.hash(right[:ksize])).sequence == right

        compactor.update_sequence(left + right)
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.query_unode_end(graph.hash(left[:ksize])).sequence == left + right
        assert compactor.cdbg.query_unode_end(graph.hash(left[-ksize:])) is None
        assert compactor.cdbg.query_unode_end(graph.hash(right[:ksize])) is None


class TestUnitigSplit(object):

    @using_ksize(15)
    @using_length(50)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_clip_from_left(self, right_sea, ksize, length, graph, compactor,
                                  check_fp):
        top, bottom = right_sea()
        check_fp()

        compactor.update_sequence(top)
        compactor.update_sequence(bottom)

        assert compactor.cdbg.n_dnodes == 1
        assert compactor.cdbg.n_unodes == 2
        assert compactor.cdbg.n_unitig_ends == 4

        assert compactor.cdbg.query_dnode(graph.hash(top[:ksize])).sequence == top[:ksize]
        
        top_unode = compactor.cdbg.query_unode_end(graph.hash(top[1:ksize+1]))
        assert top_unode is not None
        assert top_unode.sequence == top[1:]
        assert top_unode.right_end == graph.hash(top[-ksize:])
        
        bottom_unode = compactor.cdbg.query_unode_end(graph.hash(bottom[1:ksize+1]))
        assert bottom_unode is not None
        assert bottom_unode.sequence == bottom[1:]
        assert bottom_unode.right_end == graph.hash(bottom[-ksize:])

    @using_ksize(15)
    @using_length(50)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_clip_from_right(self, left_sea, ksize, length, graph, compactor,
                                   check_fp):
        top, bottom = left_sea()
        check_fp()

        compactor.update_sequence(top)
        compactor.update_sequence(bottom)

        assert compactor.cdbg.n_dnodes == 1
        assert compactor.cdbg.n_unodes == 2
        assert compactor.cdbg.n_unitig_ends == 4

        assert compactor.cdbg.query_dnode(graph.hash(top[-ksize:])).sequence == top[-ksize:]
        
        top_unode = compactor.cdbg.query_unode_end(graph.hash(top[-(ksize+1):-1]))
        assert top_unode is not None
        assert top_unode.sequence == top[:-1]
        assert top_unode.left_end == graph.hash(top[:ksize])
        
        bottom_unode = compactor.cdbg.query_unode_end(graph.hash(bottom[-(ksize+1):-1]))
        assert bottom_unode is not None
        assert bottom_unode.sequence == bottom[:-1]
        assert bottom_unode.left_end == graph.hash(bottom[:ksize])

    @using_ksize(15)
    @using_length(150)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_induced_decision_to_unitig_extend(self, ksize, length, graph, compactor,
                                                     right_fork, check_fp):
        (core, branch), pos = right_fork()
        check_fp()

        to_be_end_induced = core[ksize+2:pos+1] + branch
        waist_left = core[:ksize+2]
        waist_right = core[pos+ksize:]

        compactor.update_sequence(to_be_end_induced)
        assert compactor.cdbg.n_dnodes == 0
        assert compactor.cdbg.n_unodes == 1

        compactor.update_sequence(core)
        assert compactor.cdbg.n_dnodes == 1
        assert compactor.cdbg.n_unodes == 3
        assert compactor.cdbg.n_unitig_ends == 6

        assert compactor.cdbg.query_dnode(graph.hash(core[pos:pos+ksize])).sequence == \
               core[pos:pos+ksize]

        assert compactor.cdbg.query_unode_end(graph.hash(core[:ksize])).right_end == \
               graph.hash(core[pos-1:pos+ksize-1])

        assert compactor.cdbg.query_unode_end(graph.hash(branch[:ksize])).right_end == \
               graph.hash(branch[-ksize:])
        
        assert compactor.cdbg.query_unode_end(graph.hash(core[pos+1:pos+ksize+1])).right_end == \
               graph.hash(core[-ksize:])
    
    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_left_induced_split(self, ksize, length, graph, compactor,
                                                   left_fork, check_fp):
        ''' Decision node is induced by a non-decision segment end,
            with flanking known sequence to its  right

            (begin)-[S]-[D x]-(end)
        '''
        (core, branch), pos = left_fork()
        print(core, branch, sep='\n')
        print(core[pos:pos+ksize])

        compactor.update_sequence(core)
        assert compactor.cdbg.n_dnodes == 0
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.n_unitig_ends == 2

        compactor.update_sequence(branch)
        assert compactor.cdbg.n_dnodes == 1
        assert compactor.cdbg.n_unodes == 3
        assert compactor.cdbg.n_unodes == 3
        assert compactor.cdbg.n_unitig_ends == 6

        branch_unode = compactor.cdbg.query_unode_end(graph.hash(branch[:ksize]))
        assert branch_unode is not None
        assert branch_unode.sequence == branch
        assert branch_unode.left_end == graph.hash(branch[:ksize])
        assert branch_unode.right_end == graph.hash(branch[-ksize:])
    
        assert core[pos:pos+ksize] not in branch_unode.sequence
        assert compactor.cdbg.query_dnode(graph.hash(core[pos:pos+ksize])) is not None

        core_left_unode = compactor.cdbg.query_unode_end(graph.hash(core[:ksize]))
        assert core_left_unode is not None
        assert core_left_unode.sequence == core[:pos+ksize-1]
        assert core_left_unode.left_end == graph.hash(core[:ksize])
        assert core_left_unode.right_end == graph.hash(core[pos-1:pos+ksize-1])

        core_right_unode = compactor.cdbg.query_unode_end(graph.hash(core[-ksize:]))
        assert core_right_unode.sequence == core[pos+1:]
        assert core_right_unode.left_end == graph.hash(core[pos+1:pos+ksize+1])
        assert core_right_unode.right_end == graph.hash(core[-ksize:])


    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_right_induced_split(self, ksize, length, graph, compactor,
                                                    right_fork, check_fp):
        ''' Decision node is induced by a non-decision segment end,
            with flanking known sequence to its  right

            (begin)-[x D]-[S]-(end)
        '''
        (core, branch), pos = right_fork()
        print(core, branch, sep='\n')
        print(core[pos:pos+ksize])

        compactor.update_sequence(core)
        assert compactor.cdbg.n_dnodes == 0
        assert compactor.cdbg.n_unodes == 1
        assert compactor.cdbg.n_unitig_ends == 2

        compactor.update_sequence(branch)
        assert compactor.cdbg.n_dnodes == 1
        assert compactor.cdbg.n_unodes == 3
        assert compactor.cdbg.n_unitig_ends == 6

        branch_unode = compactor.cdbg.query_unode_end(graph.hash(branch[:ksize]))
        assert branch_unode is not None
        assert branch_unode.sequence == branch
        assert branch_unode.left_end == graph.hash(branch[:ksize])
        assert branch_unode.right_end == graph.hash(branch[-ksize:])
    
        assert core[pos:pos+ksize] not in branch_unode.sequence
        assert compactor.cdbg.query_dnode(graph.hash(core[pos:pos+ksize])) is not None

        core_left_unode = compactor.cdbg.query_unode_end(graph.hash(core[:ksize]))
        assert core_left_unode is not None
        assert core_left_unode.sequence == core[:pos+ksize-1]
        assert core_left_unode.left_end == graph.hash(core[:ksize])
        assert core_left_unode.right_end == graph.hash(core[pos-1:pos+ksize-1])

        core_right_unode = compactor.cdbg.query_unode_end(graph.hash(core[-ksize:]))
        assert core_right_unode.sequence == core[pos+1:]
        assert core_right_unode.left_end == graph.hash(core[pos+1:pos+ksize+1])
        assert core_right_unode.right_end == graph.hash(core[-ksize:])

    @using_ksize(15)
    @using_length(100)
    @pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
    def test_tandem_decision_unitig_clipping(self, ksize, length, graph, compactor,
                                          tandem_quad_forks, check_fp):
        (core, left_branches, right_branches), left_pos, right_pos = tandem_quad_forks()
        left_dkmer = core[left_pos:left_pos+ksize]
        right_dkmer = core[right_pos:right_pos+ksize]
        print('left d-node:', left_dkmer, left_pos, graph.hash(left_dkmer))
        print('right d-node:', right_dkmer, right_pos, graph.hash(right_dkmer))
        compactor.update_sequence(core)
        
        n_ends = 2
        n_unodes = 1
        for branch_num, branch in enumerate(left_branches):
            print('*** INSERT left branch', branch_num, file=sys.stderr)
            compactor.update_sequence(branch)
            if branch_num == 0:
                n_ends += 4 # the first branch induces the dnode and splits core
                n_unodes += 2
            else:
                n_ends += 2
                n_unodes += 1
            assert compactor.cdbg.n_dnodes == 1
            assert compactor.cdbg.n_unitig_ends == n_ends
            assert compactor.cdbg.n_unodes == n_unodes
            unode = compactor.cdbg.query_unode_end(graph.hash(branch[:ksize]))
            assert unode is not None
            assert unode.sequence == branch
            assert unode.right_end == graph.hash(branch[-ksize:])

        left_unode = compactor.cdbg.query_unode_end(graph.hash(core[:ksize]))
        assert left_unode is not None
        assert left_unode.right_end == graph.hash(core[left_pos-1:left_pos-1+ksize])
        assert left_unode.sequence == core[:left_pos-1+ksize]

        for branch_num, branch in enumerate(right_branches):
            print('*** INSERT right branch', branch_num, file=sys.stderr)
            compactor.update_sequence(branch)
            n_ends += 2 # first branch induces the second d-node but clips
                        # the unitig rather than splitting it
            n_unodes += 1

            assert compactor.cdbg.n_dnodes == 2
            assert compactor.cdbg.n_unitig_ends == n_ends
            assert compactor.cdbg.n_unodes == n_unodes

            left_end = graph.hash(branch[:ksize])
            unode = compactor.cdbg.query_unode_end(left_end)
            print('left_end for branch is', left_end, file=sys.stderr)

            assert branch[:ksize] not in core
            branch_hashes = list(graph.hashes(branch[:ksize+3]))
            print(branch_hashes, file=sys.stderr)
            print("branch is length ", len(branch))

            assert unode is not None
            assert unode.sequence == branch
            assert unode.right_end == graph.hash(branch[-ksize:])


@using_ksize(21)
@pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
def test_find_decision_kmers(ksize, graph, compactor, consumer, right_fork):
    (core, branch), pos = right_fork()
    print(core, ' ' * (pos + 1) + branch, sep='\n')

    positions, hashes = compactor.find_decision_kmers(core)
    assert positions == [pos]
    assert hashes == [graph.hash(core[pos:pos+ksize])]


@using_ksize(21)
@pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
def test_update_triple_fork(ksize, graph, compactor, right_triple_fork):
    (core, top, bottom), pos = right_triple_fork()
    print('\n', core, ' ' * (pos + 1) + top, sep='\n')
    compactor.update_sequence(top)
    compactor.update_sequence(bottom)
    compactor.update_sequence(core)
    compactor.wait_on_updates()

    dnode = list(compactor.get_cdbg_dnodes(core)).pop()
    assert dnode.left_degree == 1
    left_unode = list(compactor.cdbg.left_neighbors(dnode)).pop()
    print(left_unode.sequence)
    assert dnode.sequence == left_unode.sequence[-ksize:]

    assert dnode.right_degree == 3
    for unode in compactor.cdbg.right_neighbors(dnode):
        assert dnode.sequence == unode.sequence[:ksize]

    assert compactor.cdbg.n_dnodes == 1
    assert compactor.cdbg.n_unodes == 4


@using_ksize(21)
@pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
def test_double_update(ksize, graph, compactor, right_triple_fork,
                        random_sequence):
    (core, top, bottom), pos = right_triple_fork()
    print('\n', core, ' ' * (pos + 1) + top, sep='\n')


    for loop in (1,2):
        compactor.update_sequence(top)
        compactor.update_sequence(bottom)
        compactor.wait_on_updates()
        if loop == 1:
            print('CORE update', file=sys.stderr)
            compactor.update_sequence(core)
        if loop == 2:
            print('CORE2 update', file=sys.stderr)
            core = random_sequence() + core

        dnode = list(compactor.get_cdbg_dnodes(core)).pop()
        assert dnode.left_degree == 1
        left_unode = list(compactor.cdbg.left_neighbors(dnode)).pop()
        print(left_unode.sequence)
        assert dnode.sequence == left_unode.sequence[-ksize:]

        assert dnode.right_degree == 3
        for unode in compactor.cdbg.right_neighbors(dnode):
            assert dnode.sequence == unode.sequence[:ksize]

        assert compactor.cdbg.n_dnodes == 1
        assert compactor.cdbg.n_unodes == 4


@using_ksize(21)
@pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
def test_update_snp_bubble(ksize, graph, compactor, snp_bubble):
    (wildtype, snp), L, R = snp_bubble()

    compactor.update_sequence(wildtype)
    compactor.update_sequence(snp)
    compactor.wait_on_updates()
    
    assert compactor.cdbg.n_dnodes == 2
    assert compactor.cdbg.n_unodes == 4

    dnodes = list(compactor.get_cdbg_dnodes(wildtype))
    left, right = dnodes
    assert left.left_degree == 1
    assert left.right_degree == 2
    assert right.left_degree == 2
    assert right.right_degree == 1


@using_ksize(21)
@using_length(70)
@pytest.mark.parametrize('graph_type', ['_BitStorage'], indirect=['graph_type'])
def test_multi_update(ksize, graph, compactor, consumer,
                      right_fork, linear_path, snp_bubble):
    (wildtype, snp), L, R = snp_bubble()

    compactor.update_cdbg(wildtype)
    compactor.update_cdbg(snp)
    compactor.wait_on_updates()
    
    assert compactor.cdbg.n_dnodes == 2
    assert compactor.cdbg.n_unodes == 4

    dnodes = list(compactor.get_cdbg_dnodes(wildtype))
    left, right = dnodes
    assert left.left_degree == 1
    assert left.right_degree == 2
    assert right.left_degree == 2
    assert right.right_degree == 1

    wildtype_extend = wildtype[-ksize:] + linear_path()

    compactor.update_cdbg(wildtype_extend)
    compactor.wait_on_updates()

    assert compactor.cdbg.n_dnodes == 2
    assert compactor.cdbg.n_unodes == 4

    dnodes = list(compactor.get_cdbg_dnodes(wildtype_extend))
    assert len(dnodes) == 0

    (core, branch), pos = right_fork()
    compactor.update_cdbg(branch)
    compactor.wait_on_updates()
    assert compactor.cdbg.n_unodes == 7

    compactor.update_cdbg(core)
    compactor.wait_on_updates()
    dnode = list(compactor.get_cdbg_dnodes(core)).pop()
    assert dnode.left_degree == 1
    assert dnode.right_degree == 2
    assert compactor.cdbg.n_dnodes == 3

    compactor.update_sequence(wildtype_extend + core)
    compactor.wait_on_updates()

    assert compactor.cdbg.n_dnodes == 3
    assert compactor.cdbg.n_unodes == 6
