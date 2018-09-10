/* compactor.hh -- streaming dBG compactor
 *
 * Copyright (C) 2018 Camille Scott
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef COMPACTOR_HH
#define COMPACTOR_HH

#include <assert.h>

#include "boink/assembly.hh"
#include "boink/hashing.hh"
#include "boink/dbg.hh"
#include "boink/cdbg.hh"
#include "boink/minimizers.hh"
#include "boink/event_types.hh"


namespace boink {
using namespace boink::event_types;


# ifdef DEBUG_CPTR
#   define pdebug(x) do { std::cerr << std::endl << "@ " << __FILE__ <<\
                          ":" << __FUNCTION__ << ":" <<\
                          __LINE__  << std::endl << x << std::endl;\
                          } while (0)
# else
#   define pdebug(x) do {} while (0)
# endif

#define complement(ch) ((ch) == 'A' ? 'T' : \
                        (ch) == 'T' ? 'A' : \
                        (ch) == 'C' ? 'G' : 'C')

/* Packs up information on the current compaction state.
 */
struct StreamingCompactorReport {
    uint64_t n_full;
    uint64_t n_tips;
    uint64_t n_islands;
    uint64_t n_unknown;
    uint64_t n_trivial;
    uint64_t n_dnodes;
    uint64_t n_unodes;
    uint64_t n_updates;
    uint64_t n_tags;
    uint64_t n_unique;
    double   estimated_fp;
};


/* Represents a segment of new k-mers from a sequence, relative to the current
 * state of the cDBG. Can either be: a null segment representing a portion
 * of the sequence already in the graph; a decision k-mer; or a new unitig or
 * portion of a unitig.
 */
struct compact_segment {
    // anchors are:
    //    1) if the segment is a unitig segment,
    //       the left or rightmost k-mer in the segment if it has no
    //       immediate existing unitig neighbor, or the unitig end it
    //       connects to if it does have that neighbor
    //    2) if the segment is a decision k-mer, the hash of that k-mer
    //       is both left and right anchor
    hash_t left_anchor;
    hash_t right_anchor;
    hash_t left_flank;
    hash_t right_flank;

    // whether this segment represents a decision k-mer
    bool is_decision_kmer;
    // start position of the segment within the originating sequence
    size_t start_pos;
    // length of the segment sequence (from beginning of first k-mer to
    // end of last k-mer)
    size_t length;

    // tags associated with this segment
    HashVector tags;

    // the default constructor creates a null segment
    compact_segment()
        : left_anchor(0),
          right_anchor(0),
          left_flank(0),
          right_flank(0),
          is_decision_kmer(false),
          start_pos(0),
          length(0) {}

    compact_segment(hash_t left_anchor,
                    hash_t right_anchor,
                    bool is_decision_kmer,
                    size_t start_pos,
                    size_t length)
        : left_anchor(left_anchor),
          right_anchor(right_anchor),
          is_decision_kmer(is_decision_kmer),
          start_pos(start_pos),
          length(length) {}

    // return if the segment is default constructed / null
    // used as a delimiter token between connected runs of segments
    const bool is_null() const {
        return (left_anchor == right_anchor) && !is_decision_kmer;
    }
};


std::ostream& operator<<(std::ostream& os, const compact_segment& segment)
{
    os << "<compact_segment left flank=" << segment.left_flank
       << " left_anchor=" << segment.left_anchor
       << " right_anchor=" << segment.right_anchor
       << " right_flank=" << segment.right_flank
       << " start=" << segment.start_pos
       << " length=" << segment.length
       << ">";
    return os;
}


template <class GraphType>
class StreamingCompactor : public AssemblerMixin<GraphType>,
                           public EventNotifier {

protected:

    uint64_t _minimizer_window_size;
    bool _cdbg_external;

public:

    using ShifterType = typename GraphType::shifter_type;
    using AssemblerType = AssemblerMixin<GraphType>;
    using AssemblerType::seen;
    using AssemblerType::get_left;
    using AssemblerType::get_right;
    using AssemblerType::degree_left;
    using AssemblerType::degree_right;
    using AssemblerType::count_nodes;
    using AssemblerType::filter_nodes;
    using AssemblerType::find_left_kmers;
    using AssemblerType::find_right_kmers;
    using AssemblerType::gather_left;
    using AssemblerType::gather_right;

    GraphType * dbg;
    cDBG * cdbg;

    StreamingCompactor(GraphType * dbg,
                       uint64_t minimizer_window_size=8,
                       cDBG * cdbg=nullptr)
        : AssemblerMixin<GraphType>(dbg),
          EventNotifier(),
          _minimizer_window_size(minimizer_window_size),
          dbg(dbg)
    {
        if (cdbg == nullptr) {
            this->cdbg = new cDBG(dbg->K());
            _cdbg_external = false;
        } else {
            this->cdbg = cdbg;
            _cdbg_external = true;
        }
    }

    ~StreamingCompactor() {
        //cdbg->wait_on_processing(0);

        // make sure nothing else has a lock on the cdbg
        auto dlock = cdbg->lock_dnodes();
        auto ulock = cdbg->lock_unodes();

        if (!_cdbg_external) {
            delete cdbg;
        }
    }
    
    StreamingCompactorReport* get_report() {
        StreamingCompactorReport * report = new StreamingCompactorReport();
        report->n_full = cdbg->meta_counter.full_count;
        report->n_tips = cdbg->meta_counter.tip_count;
        report->n_islands = cdbg->meta_counter.island_count;
        report->n_unknown = cdbg->meta_counter.unknown_count;
        report->n_trivial = cdbg->meta_counter.trivial_count;
        report->n_dnodes = cdbg->n_decision_nodes();
        report->n_unodes = cdbg->n_unitig_nodes();
        report->n_tags = cdbg->n_tags();
        report->n_updates = cdbg->n_updates();
        report->n_unique = dbg->n_unique();
        report->estimated_fp = dbg->estimated_fp();

        return report;
    }

    string compactify(const string& seed) {
        Path path;
        this->set_cursor(seed);
        this->get_cursor(path);
        hash_t end_hash;
        compactify_left(path, end_hash);
        this->set_cursor(seed);
        compactify_right(path, end_hash);

        return this->to_string(path);
    }

    void compactify_right(Path& path, hash_t& end_hash, set<hash_t>& mask) {
        end_hash = this->get();
        this->seen.clear();
        this->seen.insert(this->get());
        
        shift_t next;
        uint8_t n_left, n_right;
        while (1) {
            if (degree_left() > 1) {
                path.pop_back();
                return;
            }

            n_right = this->reduce_nodes(this->gather_right(), next);
            if (n_right > 1) {
                path.pop_back();
                return;
            }

            if (n_right == 0) {
                end_hash = this->get();
                return;
            }

            if (this->seen.count(next.hash) ||
                mask.count(next.hash)) {
                end_hash = this->get();
                return;
            }

            end_hash = this->get();
            this->shift_right(next.symbol);
            path.push_back(next.symbol);
            this->seen.insert(next.hash);
        }
    }

    void compactify_left(Path& path, hash_t& end_hash, set<hash_t>& mask) {
        end_hash = this->get();
        this->seen.clear();
        this->seen.insert(this->get());

        shift_t next;
        uint8_t n_left, n_right;
        while (1) {
            if (degree_right() > 1) {
                pdebug("Stop: reverse d-node");
                path.pop_front();
                return;
            }

            n_left = this->reduce_nodes(this->gather_left(), next);
            if (n_left > 1) {
                pdebug("Stop: forward d-node");
                path.pop_front();
                return;
            }

            if (n_left == 0) {
                end_hash = this->get();
                return;
            }

            if (this->seen.count(next.hash) ||
                mask.count(next.hash)) {
                end_hash = this->get();
                return;
            }
            
            end_hash = this->get();
            this->shift_left(next.symbol);
            path.push_front(next.symbol);
            this->seen.insert(next.hash);
        }
    }

    bool is_decision_kmer(const string& node,
                          uint8_t& degree) {
        this->set_cursor(node);
        return is_decision_kmer(degree);
    }

    bool is_decision_kmer(const string& node) {
        this->set_cursor(node);
        return this->degree_left() > 1 || this->degree_right() > 1;
    }

    bool is_decision_kmer(uint8_t& degree) {
        uint8_t ldegree, rdegree;
        ldegree = this->degree_left();
        rdegree = this->degree_right();
        degree = ldegree + rdegree;
        return ldegree > 1 || rdegree > 1;
    }

    void find_decision_kmers(const string& sequence,
                             vector<uint32_t>& decision_positions,
                             HashVector& decision_hashes,
                             vector<NeighborBundle>& decision_neighbors) {

        KmerIterator<AssemblerType> iter(sequence, this);
        size_t pos = 0;
        while(!iter.done()) {
            hash_t h = iter.next();
            NeighborBundle neighbors;
            if (get_decision_neighbors(iter.shifter,
                                       sequence.substr(pos, this->_K),
                                       neighbors)) {

                decision_neighbors.push_back(neighbors);
                decision_positions.push_back(pos);
                decision_hashes.push_back(h);
            }
        
            ++pos;
       }
    }

    bool get_decision_neighbors(const string& root,
                                NeighborBundle& result) {
        return get_decision_neighbors(this, root, result);
    }

    bool get_decision_neighbors(NeighborBundle& result) {
        return get_decision_neighbors(this, result);
    }

    bool get_decision_neighbors(AssemblerType* shifter,
                                const string& root,
                                NeighborBundle& result) {
        shifter->set_cursor(root);
        return get_decision_neighbors(shifter, result);
    }

    bool get_decision_neighbors(AssemblerType* shifter,
                                NeighborBundle& result) {

        auto left_kmers = shifter->find_left_kmers();
        auto right_kmers = shifter->find_right_kmers();
        
        if (left_kmers.size() > 1 || right_kmers.size() > 1) {
            result = std::make_pair(left_kmers, right_kmers);
            return true;
        } else {
            return false;
        }
    }

    compact_segment init_segment(hash_t left_anchor,
                                 hash_t left_flank,
                                 size_t start_pos) {
        compact_segment segment;
        segment.start_pos = start_pos;
        segment.left_anchor = left_anchor;
        segment.left_flank = left_flank;
        segment.is_decision_kmer = false;

        return segment;
    }

    void finish_segment(compact_segment& segment,
                        size_t end,
                        hash_t right_anchor,
                        hash_t right_flank,
                        deque<compact_segment>& segments) {
        
        segment.length = end - segment.start_pos + this->_K;
        segment.right_anchor = right_anchor;
        segment.right_flank = right_flank;
        segments.push_back(segment);
        pdebug("Finished segment: " << segment);
    }

    void finish_decision_segment(compact_segment& segment,
                                 deque<compact_segment>& segments) {
        segment.length = this->_K;
        segment.right_anchor = segment.left_anchor;
        segment.right_flank = segment.left_flank;
        segment.is_decision_kmer = true;
        segments.push_back(segment);

        pdebug("Finished decision segment: " << segment);       
    }

    void find_new_segments(const string& sequence,
                           set<hash_t>& new_kmers,
                           deque<compact_segment>& segments,
                           set<hash_t>& new_decision_kmers,
                           deque<NeighborBundle>& decision_neighbors
                           ) {


        vector<bool> kmer_new;
        vector<hash_t> hashes;
        dbg->add_sequence(sequence, hashes, kmer_new);

        std::ostringstream os;
        os << "k-mers: [";
        for (size_t i = 0; i < kmer_new.size(); ++i) {
            os << kmer_new[i] << ":" << hashes[i] << ",";
        }
        os << "]";
        pdebug(os.str());
        
        size_t pos = 0;
        hash_t prev_hash = hashes.front();
        bool cur_new = false, prev_new = false, is_decision = false, 
             prev_decision = false;
        string kmer_seq;

        compact_segment current_segment;
        this->set_cursor(sequence);

        segments.push_back(compact_segment()); // place a null segment
        for (auto cur_hash : hashes) {
            cur_new = kmer_new[pos];

            if(cur_new) {
                new_kmers.insert(cur_hash);
                kmer_seq = sequence.substr(pos, this->_K);

                if(!prev_new || prev_decision) {
                    pdebug("old -> new, or prev d-kmer (pos=" << pos << ")");
                    this->set_cursor(kmer_seq);
                    
                    hash_t left_flank = prev_hash;
                    if (pos == 0) {
                        vector<shift_t> lneighbors = filter_nodes(gather_left());
                        if (lneighbors.size() == 1) {
                            left_flank = lneighbors.front().hash;
                        }
                    }
                    
                    current_segment = init_segment(cur_hash, left_flank, pos);

                } else {
                    this->shift_right(kmer_seq.back());
                }
                
                NeighborBundle neighbors;
                neighbors.first.clear();
                neighbors.second.clear();
                if ((is_decision = get_decision_neighbors(kmer_seq,
                                                          neighbors)) == true) {
                    decision_neighbors.push_back(neighbors);
                }
                if(is_decision) {
                    pdebug("new k-mer & decision " << this->get_cursor()
                           << ", " << kmer_seq);
                    new_decision_kmers.insert(cur_hash);

                    if (pos > 0 && prev_new && !prev_decision) {
                        finish_segment(current_segment,
                                       pos - 1,
                                       prev_hash,
                                       cur_hash,
                                       segments);
                    }

                    auto decision_segment = init_segment(cur_hash,
                                                         prev_hash,
                                                         pos);
                    finish_decision_segment(decision_segment,
                                            segments);
                }
            } else if (prev_new) {
                pdebug("new -> old");

                finish_segment(current_segment, 
                               pos - 1,
                               prev_hash,
                               cur_hash,
                               segments);
                segments.push_back(compact_segment()); // null segment
                is_decision = false;
            } 

            ++pos;
            prev_hash = cur_hash;
            prev_new = cur_new;
            prev_decision = is_decision;
        }

        // make sure we close current segment if necessary
        if (cur_new && !prev_decision) {
            hash_t right_flank = hashes.back();
            vector<shift_t> rneighbors = filter_nodes(gather_right());
            if (rneighbors.size() == 1) {
                right_flank = rneighbors.front().hash;
            }
            pdebug("sequence ended on new k-mer");
            finish_segment(current_segment,
                           pos - 1, // we incr'd pos...
                           hashes.back(),
                           right_flank,
                           segments);
        }

        if (cur_new) {
            segments.push_back(compact_segment()); // null segment
        }

        //pdebug("Segments: " << new_segment_sequences);
    }

   void update_from_segments(const string& sequence,
                              set<hash_t>& new_kmers,
                              deque<compact_segment>& segments,
                              set<hash_t> & new_decision_kmers,
                              deque<NeighborBundle>& decision_neighbors
                              ) {

        if (segments.size() < 3) {
            pdebug("No segments.");
            return;
        }

        pdebug(segments.size() << " segments.");

        // First find all induced decision k-kmers
        // Have to wait until all are found to proceed with induction process
        // in the cDBG though
        DecisionKmerSet induced;
        size_t i = 2;
        while (i < segments.size()) {
            auto u = segments.at(i-2);
            auto v = segments.at(i-1);
            auto w = segments.at(i);
            if (v.is_null()) {
                ++i;
                continue;
            } else if (v.is_decision_kmer) {
                kmer_t decision_kmer(v.left_anchor,
                                     sequence.substr(v.start_pos, this->_K));
                _build_dnode(decision_kmer);
                _find_induced_decision_nodes(decision_kmer,
                                             decision_neighbors.front(),
                                             new_kmers,
                                             induced);
                decision_neighbors.pop_front();
            } else {
                // v is a regular segment
                // if u is null, then we need to check for left induced d-nodes
                // or for a unitig connection
                if (u.is_null()) {

                    kmer_t root(v.left_anchor,
                                sequence.substr(v.start_pos, this->_K));
                    // TODO: future optimization. we know root is not a decision
                    // k-mer, so in this case, unless v starts at pos = 0 in the sequence,
                    // the only neighbor is the preceeding k-mer in the sequence
                    _find_induced_decision_nodes_left(root, new_kmers, induced);

                }

                // v is regular segment: if w is null, check for right induced
                // d-nodes or for a unitig connection
                if (w.is_null()) {

                    kmer_t root(v.right_anchor,
                                sequence.substr(v.start_pos + v.length - this->_K, this->_K));
                    // TODO: future optimization. we know root is not a decision
                    // k-mer, so in this case, unless v starts at pos = 0 in the sequence,
                    // the only neighbor is the preceeding k-mer in the sequence
                    _find_induced_decision_nodes_right(root, new_kmers, induced);
                }
            }
            ++i;
        }

        // Induce all the decision k-mers we found
        _induce_decision_nodes(induced, new_kmers);

        // Now, with the cDBG in a correct state, update its unitigs
        // from our new segments
        for (auto segment : segments) {
            if (!segment.is_decision_kmer && !segment.is_null()) {
                _update_unode(segment, sequence);
            }
        }
    }

    void update_sequence(const string& sequence) {
        set<hash_t> new_kmers;
        deque<compact_segment> segments;
        set<hash_t> new_decision_kmers;
        deque<NeighborBundle> decision_neighbors;

        find_new_segments(sequence,
                          new_kmers,
                          segments,
                          new_decision_kmers,
                          decision_neighbors);

        update_from_segments(sequence,
                             new_kmers,
                             segments,
                             new_decision_kmers,
                             decision_neighbors);
    }

    void _find_induced_decision_nodes(kmer_t kmer,
                                      NeighborBundle& neighbors,
                                      set<hash_t>& neighbor_mask,
                                      DecisionKmerSet& induced) {

        _find_induced_decision_nodes_left(kmer, neighbors, neighbor_mask, induced);
        _find_induced_decision_nodes_right(kmer, neighbors, neighbor_mask, induced);
    }

    uint8_t _find_induced_decision_nodes_left(kmer_t kmer,
                                              set<hash_t>& neighbor_mask,
                                              DecisionKmerSet& induced) {

        pdebug("Prepare to attempt left induction on " << kmer);
        this->set_cursor(kmer.kmer);
        NeighborBundle bundle;
        bundle.first = find_left_kmers();

        if (bundle.first.size()) {
            return _find_induced_decision_nodes_left(kmer,
                                                     bundle,
                                                     neighbor_mask,
                                                     induced);
        }
        return 0;
    }

    uint8_t _find_induced_decision_nodes_left(kmer_t kmer,
                                              NeighborBundle& neighbors,
                                              set<hash_t>& neighbor_mask,
                                              DecisionKmerSet& induced) {

        // decision k-mers which are also new k-mers
        // cannot split existing unitigs. however, they can induce
        // an existing k-mer to be a decision k-mer, which can split
        // existing unitigs. so, we filter out neighbors of
        // the new decision k-mer which already exist and are already known
        // to the cDBG to be decision k-mers

        pdebug("Attempt left d-node induction from " << kmer);

        uint8_t n_found = 0;
        for (auto lneighbor : neighbors.first) {
            if (neighbor_mask.count(lneighbor.hash) ||
                cdbg->has_dnode(lneighbor.hash)) {
                continue;
            }
            NeighborBundle inductee_neighbors;
            if (get_decision_neighbors(lneighbor.kmer,
                                       inductee_neighbors)) {

                pdebug("Found induced d-node: " << lneighbor.hash << ", " << lneighbor.kmer);
                induced.insert(make_pair(lneighbor, inductee_neighbors));
                ++n_found;
                //_build_dnode(lneighbor);
                //_split_unode(lneighbor, neighbors, neighbor_mask); 
            }
            inductee_neighbors.first.clear();
            inductee_neighbors.second.clear();
        }

        return n_found;
    }

    uint8_t _find_induced_decision_nodes_right(kmer_t kmer,
                                         set<hash_t>& neighbor_mask,
                                         DecisionKmerSet& induced) {

        this->set_cursor(kmer.kmer);
        NeighborBundle bundle;
        bundle.second = find_right_kmers();
        
        if (bundle.second.size()) {
            return _find_induced_decision_nodes_right(kmer,
                                                      bundle,
                                                      neighbor_mask,
                                                      induced);
        }
        return 0;
    }

    uint8_t _find_induced_decision_nodes_right(kmer_t kmer,
                                         NeighborBundle& neighbors,
                                         set<hash_t>& neighbor_mask,
                                         DecisionKmerSet& induced) {

        // see _induce_decision_nodes_left for information

        pdebug("Attempt right d-node induction from " << kmer.kmer
                << ", " << kmer.hash);

        uint8_t n_found = 0;
        for (auto rneighbor : neighbors.second) {
            if (neighbor_mask.count(rneighbor.hash) ||
                cdbg->has_dnode(rneighbor.hash)) {
                continue;
            }

            NeighborBundle inductee_neighbors;
            if (get_decision_neighbors(rneighbor.kmer,
                                       inductee_neighbors)) {
                // induced decision k-mer
                pdebug("Found induced d-node: " << rneighbor.hash << ", " << rneighbor.kmer);
                induced.insert(make_pair(rneighbor, inductee_neighbors));
                ++n_found;
                //_build_dnode(rneighbor);
                //_split_unode(rneighbor, neighbors, neighbor_mask); 
            }
            inductee_neighbors.first.clear();
            inductee_neighbors.second.clear();
        }

        return n_found;
    }

    void _induce_decision_nodes(DecisionKmerSet& induced_decision_kmers,
                                set<hash_t>& new_kmers) {

        set<hash_t> induced_decision_kmer_hashes;

        pdebug("Perform induction on " << induced_decision_kmers.size() <<
               " new decision k-mers");
        for (auto dkmer : induced_decision_kmers) {
            _build_dnode(dkmer.first);
            induced_decision_kmer_hashes.insert(dkmer.first.hash);
        }
        for (auto dkmer : induced_decision_kmers) {
            _split_unode(dkmer.first, dkmer.second, new_kmers, induced_decision_kmer_hashes);
        }
    }

    virtual void _split_unode(kmer_t root,
                              NeighborBundle& neighbors,
                              set<hash_t>& mask,
                              set<hash_t>& induced_decision_kmer_hashes) {
        pdebug("Attempt unitig split from " << root);

        UnitigNode * unode;
        if ((unode = cdbg->query_unode_end(root.hash)) != nullptr) {
            // special case: induced an end k-mer, just have to trim the u-node,
            // no need to create a new one
            hash_t new_end;
            direction_t clip_from;
            if (root.hash == unode->left_end()) {
                new_end = this->hash(unode->sequence.c_str() + 1);
                clip_from = DIR_LEFT;
            } else {
                new_end = this->hash(unode->sequence.c_str() + unode->sequence.size()
                                     - this->_K - 1);
                clip_from = DIR_RIGHT;
            }
            cdbg->clip_unode(clip_from,
                             root.hash,
                             new_end);
            return;
        }

        vector<kmer_t> lfiltered;
        std::copy_if(neighbors.first.begin(),
                     neighbors.first.end(),
                     std::back_inserter(lfiltered),
                     [&] (kmer_t neighbor) { return
                        !mask.count(neighbor.hash) &&
                        !induced_decision_kmer_hashes.count(neighbor.hash);
                     });

        vector<kmer_t> rfiltered;
        std::copy_if(neighbors.second.begin(),
                     neighbors.second.end(),
                     std::back_inserter(rfiltered),
                     [&] (kmer_t neighbor) { return
                        !mask.count(neighbor.hash) &&
                        !induced_decision_kmer_hashes.count(neighbor.hash);
                     });

        pdebug(lfiltered.size() << " left, " << rfiltered.size() << " right");
        if (lfiltered.size()) {
            // size should always be 1 here
            pdebug("Found a valid left neighbor, search this way... ("
                   << lfiltered.size() << " in filtered set, should always be 1.)");
            auto start = lfiltered.back();
            this->set_cursor(start.kmer);
            Path path;
            hash_t end_hash;
            compactify_left(path, end_hash, mask);

            auto unode = cdbg->query_unode_end(end_hash);
            assert(unode != nullptr);

            size_t split_point = path.size() + 1;
            hash_t left_unode_new_right = start.hash;
            pdebug("split point is " << split_point << " new_right is " << left_unode_new_right
                   << " root was " << root.hash);
            hash_t right_unode_new_left = this->hash(unode->sequence.c_str() + split_point + 1);

            cdbg->split_unode(unode->node_id,
                              split_point,
                              left_unode_new_right,
                              right_unode_new_left);

            return;
        }


        if (rfiltered.size()) {
            // size should always be 1 here
            pdebug("Found a valid left neighbor, search this way... ("
                   << rfiltered.size() << " in filtered set, should be 1.");
            auto start = rfiltered.back();
            this->set_cursor(start.kmer);
            Path path;
            hash_t end_hash;
            compactify_right(path, end_hash, mask);

            auto unode = cdbg->query_unode_end(end_hash);
            assert(unode != nullptr);
            
            size_t split_point = unode->sequence.size() - path.size() - 2;
            hash_t new_right = this->hash(unode->sequence.c_str() + split_point - 1);
            hash_t new_left = start.hash;

            cdbg->split_unode(unode->node_id,
                              split_point,
                              new_right,
                              new_left);

            return;
        }
    }

    virtual void _update_unode(compact_segment& segment,
                               const string& sequence) {

        pdebug("Update Unode from segment: " << segment);

        bool has_left_unode = cdbg->has_unode_end(segment.left_flank);
        bool has_right_unode = cdbg->has_unode_end(segment.right_flank);
        
        if (has_left_unode && !has_right_unode) {
            auto trimmed_seq = sequence.substr(segment.start_pos + this->_K - 1,
                                               segment.length - this->_K + 1);
            cdbg->extend_unode(DIR_RIGHT,
                               trimmed_seq,
                               segment.left_flank,
                               segment.right_anchor,
                               segment.tags);

        } else if (!has_left_unode && has_right_unode) {
            auto trimmed_seq = sequence.substr(segment.start_pos,
                                               segment.length - this->_K + 1);
            cdbg->extend_unode(DIR_LEFT,
                               trimmed_seq,
                               segment.right_flank,
                               segment.left_anchor,
                               segment.tags);
        } else if (has_left_unode && has_right_unode) {
            auto trimmed_seq = sequence.substr(segment.start_pos + this->_K - 1,
                                               segment.length - (this->_K * 2 - 2));
            cdbg->merge_unodes(trimmed_seq,
                               segment.left_flank,
                               segment.right_flank,
                               segment.tags);
        } else {
            cdbg->build_unode(sequence.substr(segment.start_pos, segment.length),
                              segment.tags,
                              segment.left_anchor,
                              segment.right_anchor);
        }
    }

    virtual void _build_dnode(kmer_t kmer) {
        cdbg->build_dnode(kmer.hash, kmer.kmer);
    }
};


template <class GraphType>
class AsyncStreamingCompactor : public StreamingCompactor<GraphType>,
                                public EventNotifier {

public:

    using ShifterType = typename GraphType::shifter_type;
    using AssemblerType = AssemblerMixin<GraphType>;
    using AssemblerType::seen;
    using AssemblerType::get_left;
    using AssemblerType::get_right;
    using AssemblerType::degree_left;
    using AssemblerType::degree_right;
    using AssemblerType::count_nodes;
    using AssemblerType::filter_nodes;
    using AssemblerType::find_left_kmers;
    using AssemblerType::find_right_kmers;
    using AssemblerType::gather_left;
    using AssemblerType::gather_right;

    AsyncCDBG * acdbg;

    AsyncStreamingCompactor(GraphType * dbg,
                            uint64_t minimizer_window_size=8)
        : EventNotifier()
    {
        register_listener(static_cast<EventListener*>(acdbg));
        acdbg = new AsyncCDBG(dbg->K());
        StreamingCompactor<GraphType>::StreamingCompactor(dbg, minimizer_window_size, acdbg);
    }

    ~AsyncStreamingCompactor() {
        _cerr("AsyncStreamingCompactor: waiting for cDBG to finish updating.");
        //cdbg->wait_on_processing(0);

        delete acdbg;
    }

    void wait_on_updates() {
        acdbg->wait_on_processing(0);
    }

    void notify_build_dnode(hash_t hash, const string& kmer) {
        auto event = make_shared<BuildDNodeEvent>();
        event->hash = hash;
        event->kmer = kmer;
        this->notify(event);
    }

    void notify_build_unode(const string& sequence,
                            HashVector& tags,
                            hash_t left_end,
                            hash_t right_end) {
        auto event = make_shared<BuildUNodeEvent>();
        event->tags = tags;
        event->sequence = sequence;
        event->left_end = left_end;
        event->right_end = right_end;
        this->notify(event);
    }

    void notify_delete_unode(id_t node_id) {
        auto event = make_shared<DeleteUNodeEvent>();
        event->node_id = node_id;
        this->notify(event);
    }
};
}
#undef pdebug
#endif
