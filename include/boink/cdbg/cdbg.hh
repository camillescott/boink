/**
 * (c) Camille Scott, 2019
 * File   : cdbg.hh
 * License: MIT
 * Author : Camille Scott <camille.scott.w@gmail.com>
 * Date   : 31.08.2019
 */


#ifndef BOINK_CDBG_HH
#define BOINK_CDBG_HH

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <memory>
#include <mutex>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>

// save diagnostic state
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#include "boink/storage/sparsepp/spp.h"
#pragma GCC diagnostic pop

#include "boink/boink.hh"

#include "boink/events.hh"
#include "boink/event_types.hh"
#include "boink/metrics.hh"
#include "boink/reporting/reporters.hh"

#include "boink/traversal.hh"
#include "boink/hashing/kmeriterator.hh"
#include "boink/hashing/hashextender.hh"
#include "boink/kmers/kmerclient.hh"
#include "boink/storage/storage.hh"

#include "boink/cdbg/cdbg_types.hh"
#include "boink/cdbg/metrics.hh"


# ifdef DEBUG_CDBG
#   define pdebug(x) do { std::ostringstream stream; \
                          stream << std::endl << "@ " << __FILE__ <<\
                          ":" << __FUNCTION__ << ":" <<\
                          __LINE__  << std::endl << x << std::endl;\
                          std::cerr << stream.str(); \
                          } while (0)
# else
#   define pdebug(x) do {} while (0)
# endif


namespace boink {
namespace cdbg {


template <class GraphType>
struct cDBG {

public:

    typedef GraphType                           graph_type;

    typedef typename graph_type::shifter_type   shifter_type;
    typedef hashing::HashExtender<shifter_type> extender_type;
    typedef typename shifter_type::alphabet     alphabet;
    typedef typename shifter_type::hash_type    hash_type;
	typedef typename hash_type::value_type      value_type;
    typedef typename shifter_type::kmer_type    kmer_type;

    typedef dBGWalker<graph_type>               walker_type;


    class CompactNode {

    protected:

        node_meta_t _meta;

    public:

        const id_t node_id;
        id_t component_id;
        std::string sequence;
        
        CompactNode(id_t node_id,
                    const std::string& sequence,
                    node_meta_t meta)
            : _meta(meta),
              node_id(node_id),
              component_id(NULL_ID),
              sequence(sequence)
        {
        }

        std::string revcomp() const {
            return alphabet::reverse_complement(sequence);
        }

        size_t length() const {
            return sequence.length();
        }

        const node_meta_t meta() const {
            return _meta;
        }

        friend bool operator== (const CompactNode& lhs, const CompactNode& rhs) {
            return lhs.node_id == rhs.node_id;
        }

        std::string get_name() const {
            return std::string("NODE") + std::to_string(node_id);
        }

    };


    class DecisionNode: public CompactNode {

    protected:

        bool _dirty;
        uint8_t _left_degree;
        uint8_t _right_degree;
        uint32_t _count;

    public:

        DecisionNode(id_t node_id, const std::string& sequence)
            : CompactNode(node_id, sequence, DECISION),
              _dirty(true),
              _left_degree(0),
              _right_degree(0),
              _count(1)
        {    
        }

        static std::shared_ptr<DecisionNode> build(const DecisionNode& other) {
            return std::make_shared<DecisionNode>(other.node_id, other.sequence);
        }

        static std::shared_ptr<DecisionNode> build(const DecisionNode * other) {
            return std::make_shared<DecisionNode>(other->node_id, other->sequence);
        }

        const bool is_dirty() const {
            return _dirty;
        }

        void set_dirty(bool dirty) {
            _dirty = dirty;
        }

        const uint32_t count() const {
            return _count;
        }

        void incr_count() {
            _count++;
        }

        const uint8_t degree() const {
            return left_degree() + right_degree();
        }

        const uint8_t left_degree() const {
            return _left_degree;
        }

        void incr_left_degree() {
            _left_degree++;
        }

        const uint8_t right_degree() const {
            return _right_degree;
        }

        void incr_right_degree() {
            _right_degree++;
        }

        std::string repr() const {
            std::ostringstream os;
            os << *this;
            return os.str();
        }

        friend inline std::ostream& operator<<(std::ostream& o, const DecisionNode& dn) {

            o << "<DNode ID/hash=" << dn.node_id << " k-mer=" << dn.sequence
              //<< " Dl=" << std::to_string(dn.left_degree())
              //<< " Dr=" << std::to_string(dn.right_degree())
              << " count=" << dn.count()
              << " dirty=" << dn.is_dirty() << ">";
            return o;
        }
    };


    class UnitigNode : public CompactNode {

    protected:

        hash_type _left_end, _right_end;
        using CompactNode::_meta;

    public:

        using CompactNode::sequence;
        std::vector<hash_type> tags;

        UnitigNode(id_t node_id,
                   hash_type left_end,
                   hash_type right_end,
                   const std::string& sequence,
                   node_meta_t meta = ISLAND)
            : CompactNode(node_id, sequence, meta),
              _left_end(left_end),
              _right_end(right_end) { 
        }

        static std::shared_ptr<UnitigNode> build(const UnitigNode& other) {
            return std::make_shared<UnitigNode>(other.node_id,
                                                other.left_end(),
                                                other.right_end(),
                                                other.sequence,
                                                other.meta());
        }

        static std::shared_ptr<UnitigNode> build(const UnitigNode * other) {
            return std::make_shared<UnitigNode>(other->node_id,
                                                other->left_end(),
                                                other->right_end(),
                                                other->sequence,
                                                other->meta());
        }

        void set_node_meta(node_meta_t new_meta) {
            _meta = new_meta;
        }

        const hash_type left_end() const {
            return _left_end;
        }

        void set_left_end(hash_type left_end) {
            _left_end = left_end;
        }

        void extend_right(hash_type right_end, const std::string& new_sequence) {
            sequence += new_sequence;
            _right_end = right_end;
        }

        void extend_left(hash_type left_end, const std::string& new_sequence) {
            sequence = new_sequence + sequence;
            _left_end = left_end;
        }

        const hash_type right_end() const {
            return _right_end;
        }

        void set_right_end(hash_type right_end) {
            _right_end = right_end;
        }

        std::string repr() const {
            std::ostringstream os;
            os << *this;
            return os.str();
        }

        friend inline std::ostream& operator<<(std::ostream& o, const UnitigNode& un) {
            o << "<UNode ID=" << un.node_id
              << " left_end=" << un.left_end()
              << " right_end=" << un.right_end()
              << " sequence=" << un.sequence
              << " length=" << un.sequence.length()
              << " meta=" << node_meta_repr(un.meta())
              << ">";
            return o;
        }

    };

    typedef CompactNode * CompactNodePtr;
    typedef DecisionNode * DecisionNodePtr;
    typedef UnitigNode * UnitigNodePtr;

    class Graph : public kmers::KmerClient,
                  public events::EventNotifier {

        /* Map of k-mer hash --> DecisionNode. DecisionNodes take
         * their k-mer hash value as their Node ID.
         */
        typedef spp::sparse_hash_map<value_type,
                                     std::unique_ptr<DecisionNode>> dnode_map_t;
        typedef typename dnode_map_t::const_iterator dnode_iter_t;

        /* Map of Node ID --> UnitigNode. This is a container
         * for the UnitigNodes' pointers; k-mer maps are stored elsewhere,
         * mapping k-mers to Node IDs.
         */
        typedef spp::sparse_hash_map<id_t,
                                     std::unique_ptr<UnitigNode>> unode_map_t;
        typedef typename unode_map_t::const_iterator unode_iter_t;

    protected:

        // The actual k-mer hash --> DNode map
        dnode_map_t decision_nodes;

        // The actual ID --> UNode map
        unode_map_t unitig_nodes;
        // The map from Unitig end k-mer hashes to UnitigNodes
        spp::sparse_hash_map<value_type, UnitigNode*> unitig_end_map;
        // The map from dBG k-mer tags to UnitigNodes
        spp::sparse_hash_map<value_type, UnitigNode*> unitig_tag_map;

        //std::mutex dnode_mutex;
        //std::mutex unode_mutex;
        std::mutex mutex;

        // Counts the number of cDBG updates so far
        uint64_t _n_updates;
        // Counter for generating UnitigNode IDs
        uint64_t _unitig_id_counter;
        // Current number of Unitigs
        uint64_t _n_unitig_nodes;

        id_t     component_id_counter;

    public:

        std::shared_ptr<GraphType> dbg;
        std::shared_ptr<cDBGMetrics> metrics;

        Graph(std::shared_ptr<GraphType> dbg,
              uint64_t minimizer_window_size=8);

        std::unique_lock<std::mutex> lock_nodes() {
            return std::unique_lock<std::mutex>(mutex);
        }

        /* Utility methods for iterating DNode and UNode
         * data structures. Note that these are not thread-safe
         * (the caller will need to lock).
         */

        typename dnode_map_t::const_iterator dnodes_begin() const {
            return decision_nodes.cbegin();
        }

        typename dnode_map_t::const_iterator dnodes_end() const {
            return decision_nodes.cend();
        }

        typename unode_map_t::const_iterator unodes_begin() const {
            return unitig_nodes.cbegin();
        }

        typename unode_map_t::const_iterator unodes_end() const {
            return unitig_nodes.cend();
        }

        /* 
         * Accessor methods.
         */

        uint64_t n_updates() const {
            return _n_updates;
        }

        uint64_t n_unitig_nodes() const {
            return _n_unitig_nodes;
        }

        uint64_t n_decision_nodes() const {
            return decision_nodes.size();
        }

        uint64_t n_tags() const {
            return unitig_tag_map.size();
        }

        uint64_t n_unitig_ends() const {
            return unitig_end_map.size();
        }

        /* Node query methods: separate query mechanisms for
         * decision nodes and unitig nodes.
         */

        CompactNode* query_cnode(hash_type hash);

        DecisionNode* query_dnode(hash_type hash);

        std::vector<DecisionNode*> query_dnodes(const std::string& sequence);

        UnitigNode * query_unode_end(hash_type end_kmer);

        UnitigNode * query_unode_tag(hash_type hash);

        UnitigNode * query_unode_id(id_t id);

        bool has_dnode(hash_type hash);

        bool has_unode_end(hash_type end_kmer);

        CompactNode * find_rc_cnode(CompactNode * root);

        /* Neighbor-finding and traversal.
         *
         */

        std::pair<std::vector<CompactNode*>,
                  std::vector<CompactNode*>> find_dnode_neighbors(DecisionNode* dnode);

        std::pair<DecisionNode*, DecisionNode*> find_unode_neighbors(UnitigNode * unode);

        std::vector<CompactNode*> traverse_breadth_first(CompactNode* root);

        spp::sparse_hash_map<id_t, std::vector<id_t>> find_connected_components();

        /*
         * Graph Mutation
         */

        node_meta_t recompute_node_meta(UnitigNode * unode);

        UnitigNode * switch_unode_ends(hash_type old_unode_end,
                                       hash_type new_unode_end);

        DecisionNode* build_dnode(hash_type hash,
                                  const std::string& kmer);

        UnitigNode * build_unode(const std::string& sequence,
                                 std::vector<hash_type>& tags,
                                 hash_type left_end,
                                 hash_type right_end);

        void clip_unode(Direction_t clip_from,
                        hash_type old_unode_end,
                        hash_type new_unode_end);

        void extend_unode(Direction_t ext_dir,
                          const std::string& new_sequence,
                          hash_type old_unode_end,
                          hash_type new_unode_end,
                          std::vector<hash_type>& new_tags);

        void split_unode(id_t node_id,
                         size_t split_at,
                         std::string split_kmer,
                         hash_type new_right_end,
                         hash_type new_left_end);

        void merge_unodes(const std::string& span_sequence,
                          size_t n_span_kmers,
                          hash_type left_end,
                          hash_type right_end,
                          std::vector<hash_type>& new_tags);

        void delete_unode(UnitigNode * unode);
    
        void delete_unodes_from_tags(std::vector<hash_type>& tags);

        void delete_dnode(DecisionNode * dnode);

        /*
         * Event notification
         */

        void notify_history_new(id_t id, std::string& sequence, node_meta_t meta);

        void notify_history_merge(id_t lparent, id_t rparent, id_t child,
                                  std::string& sequence, node_meta_t meta);

        void notify_history_extend(id_t id, std::string& sequence, node_meta_t meta);

        void notify_history_clip(id_t id, std::string& sequence, node_meta_t meta);

        void notify_history_split(id_t parent, id_t lchild, id_t rchild,
                                  std::string& lsequence, std::string& rsequence,
                                  node_meta_t lmeta, node_meta_t rmeta);

        void notify_history_split_circular(id_t id, std::string& sequence, node_meta_t meta);


        /*
         * File output
         */

        void validate(const std::string& filename);

        void write(const std::string& filename, cDBGFormat format);

        void write(std::ofstream& out, cDBGFormat format);

        void write_fasta(const std::string& filename);

        void write_fasta(std::ofstream& out);

        void write_gfa1(const std::string& filename);

        void write_gfa1(std::ofstream& out);

        void write_graphml(const std::string& filename,
                           const std::string graph_name="cDBG");

        void write_graphml(std::ofstream& out,
                           const std::string graph_name="cDBG");
    };


    class ComponentReporter : public boink::reporting::SingleFileReporter {
    public:

        class Metrics {


            public:

                metrics::Gauge                         n_components;
                metrics::Gauge                         max_component_size;
                metrics::Gauge                         min_component_size;

                Metrics()
                    : 
                      n_components              {"size", "all_components"},
                      max_component_size        {"size", "max_component"},
                      min_component_size        {"size", "min_component"}
                {
                }
        };

    private:

        std::shared_ptr<Graph>            cdbg;

        uint64_t                          min_component;
        uint64_t                          max_component;

        // how large of a sample to take from the component size distribution
        size_t                            sample_size;
        metrics::ReservoirSample<size_t>  component_size_sample;

        std::unique_ptr<ComponentReporter::Metrics> metrics;

    public:

        ComponentReporter(std::shared_ptr<Graph>                 cdbg,
                          const std::string&                     filename,
                          size_t                                 sample_size = 10000)
            : SingleFileReporter       (filename, "cDBG::ComponentReporter"),
              cdbg                     (cdbg),
              min_component            (ULLONG_MAX),
              max_component            (0),
              sample_size              (sample_size),
              component_size_sample    (sample_size)
        {
            _cerr(this->THREAD_NAME << " reporting at MEDIUM interval.");
            this->msg_type_whitelist.insert(events::MSG_TIME_INTERVAL);
            _output_stream << "read_n,n_components,max_component,min_component,sample_size,component_size_sample" << std::endl;

            metrics = std::make_unique<ComponentReporter::Metrics>();
        }

        static std::shared_ptr<ComponentReporter> build(std::shared_ptr<Graph>                 cdbg,
                                                        const std::string&                     filename,
                                                        size_t                                 sample_size = 10000) {
            return std::make_shared<ComponentReporter>(cdbg,
                                                       filename,
                                                       sample_size);
        
        }

        virtual void handle_msg(std::shared_ptr<events::Event> event) {
             if (event->msg_type == events::MSG_TIME_INTERVAL) {
                auto _event = static_cast<events::TimeIntervalEvent*>(event.get());
                if (_event->level == events::TimeIntervalEvent::MEDIUM ||
                    _event->level == events::TimeIntervalEvent::END) {
                    
                    this->recompute_components();
                    _output_stream << _event->t << ","
                                   << component_size_sample.get_n_sampled() << ","
                                   << max_component << ","
                                   << min_component << ","
                                   << component_size_sample.get_sample_size() << ","
                                   << "\"" << repr(component_size_sample.get_result()) << "\""
                                   << std::endl;
                }
            }       
        }

        void recompute_components() {
            auto time_start = std::chrono::system_clock::now();

            component_size_sample.clear();
            auto components = cdbg->find_connected_components();
            for (auto id_comp_pair : components) {
                size_t component_size = id_comp_pair.second.size();
                component_size_sample.sample(component_size);
                max_component = (component_size > max_component) ? component_size : max_component;
                min_component = (component_size < min_component) ? component_size : min_component;
            }

            metrics->n_components.store(components.size());
            metrics->max_component_size.store(max_component);
            metrics->min_component_size.store(min_component);

            auto time_elapsed = std::chrono::system_clock::now() - time_start;
            _cerr("Finished recomputing components. Elapsed time: " <<
                  std::chrono::duration<double>(time_elapsed).count());


        }
    };

    class HistoryReporter : public reporting::SingleFileReporter {
        private:

        id_t _edge_id_counter;
        spp::sparse_hash_map<id_t, std::vector<std::string>> node_history;

        public:
        HistoryReporter(const std::string& filename)
            : SingleFileReporter(filename, "cDBG::HistoryReporter"),
              _edge_id_counter(0)
        {
            _cerr(this->THREAD_NAME << " reporting continuously.");

            this->msg_type_whitelist.insert(events::MSG_HISTORY_NEW);
            this->msg_type_whitelist.insert(events::MSG_HISTORY_SPLIT);
            this->msg_type_whitelist.insert(events::MSG_HISTORY_SPLIT_CIRCULAR);
            this->msg_type_whitelist.insert(events::MSG_HISTORY_MERGE);
            this->msg_type_whitelist.insert(events::MSG_HISTORY_EXTEND);
            this->msg_type_whitelist.insert(events::MSG_HISTORY_CLIP);
            this->msg_type_whitelist.insert(events::MSG_HISTORY_DELETE);

            _output_stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                              "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\" "
                              "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                              "xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns "
                              "http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">"
                           << std::endl // the header, open <graphml>
                           << "<graph id=\"cDBG_History_DAG\" edgedefault=\"directed\">" << std::endl
                           << "<key id=\"op\" for=\"edge\" attr.name=\"op\" attr.type=\"string\"/>" << std::endl
                           << "<key id=\"seq\" for=\"node\" attr.name=\"seq\" attr.type=\"string\"/>" << std::endl
                           << "<key id=\"meta\" for=\"node\" attr.name=\"meta\" attr.type=\"string\"/>" << std::endl
                           << "<key id=\"node_id\" for=\"node\" attr.name=\"node_id\" attr.type=\"long\"/>"
                           << std::endl; // open <graph>
        }

        static std::shared_ptr<HistoryReporter> build(const std::string& filename) {
            return std::make_shared<HistoryReporter>(filename);
        }

        virtual void handle_exit() {
            _output_stream << "</graph>" << std::endl;
            _output_stream << "</graphml>" << std::endl;
        }

        void write_node(std::string id, id_t boink_id, std::string node_meta, std::string sequence) {
            _output_stream << "<node id=\"" << id << "\">" << std::endl
                           << "    <data key=\"seq\">" << sequence << "</data>" << std::endl
                           << "    <data key=\"meta\">" << node_meta << "</data>" << std::endl
                           << "    <data key=\"node_id\">" << boink_id << "</data>" << std::endl
                           << "</node>" << std::endl;
        }

        void write_edge(std::string src, std::string dst, std::string op) {
            auto id = _edge_id_counter++;
            _output_stream << "<edge id=\"" << id << "\" source=\"" 
                           << src << "\" target=\"" << dst << "\">" << std::endl
                           << "    <data key=\"op\">" << op << "</data>" << std::endl
                           << "</edge>" << std::endl;
        }

        std::string add_node_edit(id_t node_id, cdbg::node_meta_t meta, std::string sequence) {
            auto change_num = node_history[node_id].size();
            std::string id = std::to_string(node_id) + "_" + std::to_string(change_num);
            node_history[node_id].push_back(id);
            write_node(id, node_id, std::string(node_meta_repr(meta)), sequence);
            return id;
        }

        std::string add_new_node(id_t node_id, cdbg::node_meta_t meta, std::string sequence) {
            std::string id = std::to_string(node_id) + "_0";
            if (node_history.count(node_id) == 0) {
                node_history[node_id] = std::vector<std::string>{id};
                write_node(id, node_id, std::string(node_meta_repr(meta)), sequence);
            }
            return id;
        }

        virtual void handle_msg(std::shared_ptr<events::Event> event) {
            if (event->msg_type == events::MSG_HISTORY_NEW) {
                auto _event = static_cast<HistoryNewEvent*>(event.get());
                add_new_node(_event->id, _event->meta, _event->sequence);

            } else if (event->msg_type == events::MSG_HISTORY_SPLIT) {
                auto _event = static_cast<HistorySplitEvent*>(event.get());
                
                std::string parent_id = node_history[_event->parent].back();
                std::string lid, rid;
                if (_event->lchild == _event->parent) {
                    lid = add_node_edit(_event->lchild, _event->lmeta, _event->lsequence);
                    rid = add_new_node(_event->rchild, _event->rmeta, _event->rsequence);
                } else {
                    lid = add_new_node(_event->lchild, _event->lmeta, _event->lsequence);
                    rid = add_node_edit(_event->rchild, _event->rmeta, _event->rsequence);
                }
                write_edge(parent_id, lid, std::string("SPLIT"));
                write_edge(parent_id, rid, std::string("SPLIT"));

            } else if (event->msg_type == events::MSG_HISTORY_MERGE) {
                auto _event = static_cast<HistoryMergeEvent*>(event.get());

                std::string l_parent_id = node_history[_event->lparent].back();
                std::string r_parent_id = node_history[_event->rparent].back();
                std::string child_id = add_node_edit(_event->child, _event->meta, _event->sequence);
                
                write_edge(l_parent_id, child_id, std::string("MERGE"));
                write_edge(r_parent_id, child_id, std::string("MERGE"));

            } else if (event->msg_type == events::MSG_HISTORY_EXTEND) {
                auto _event = static_cast<HistoryExtendEvent*>(event.get());

                std::string src = node_history[_event->id].back();
                std::string dst = add_node_edit(_event->id, _event->meta, _event->sequence);
                write_edge(src, dst, std::string("EXTEND"));

            } else if (event->msg_type == events::MSG_HISTORY_CLIP) {
                auto _event = static_cast<HistoryClipEvent*>(event.get());

                std::string src = node_history[_event->id].back();
                std::string dst = add_node_edit(_event->id, _event->meta, _event->sequence);
                write_edge(src, dst, std::string("CLIP"));

            } else if (event->msg_type == events::MSG_HISTORY_SPLIT_CIRCULAR) {
                auto _event = static_cast<HistorySplitCircularEvent*>(event.get());

                std::string src = node_history[_event->id].back();
                std::string dst = add_node_edit(_event->id, _event->meta, _event->sequence);
                write_edge(src, dst, std::string("SPLIT_CIRCULAR"));
            }
        }
    };


    class UnitigReporter : public reporting::SingleFileReporter {
    private:

        std::shared_ptr<Graph>        cdbg;
        std::vector<size_t>           bins;

    public:

        UnitigReporter(std::shared_ptr<Graph> cdbg,
                       const std::string&     filename,
                       std::vector<size_t>    bins)
            : SingleFileReporter       (filename, "cDBG::UnitigReporter"),
              cdbg                     (cdbg),
              bins                     (bins)
        {
            _cerr(this->THREAD_NAME << " reporting at MEDIUM interval.");
            this->msg_type_whitelist.insert(events::MSG_TIME_INTERVAL);
            
            _output_stream << "read_n";

            for (size_t bin = 0; bin < bins.size() - 1; bin++) {
                _output_stream << ", " << bins[bin] << "-" << bins[bin+1];
            }
            _output_stream << ", " << bins.back() << "-Inf";

            _output_stream << std::endl;
        }

        static std::shared_ptr<UnitigReporter> build(std::shared_ptr<Graph> cdbg,
                                                     const std::string&     filename,
                                                     std::vector<size_t>    bins) {
            return std::make_shared<UnitigReporter>(cdbg, filename, bins);
        }

        virtual void handle_msg(std::shared_ptr<events::Event> event);

        std::vector<size_t> compute_bins() {
            auto time_start = std::chrono::system_clock::now();
            auto lock       = cdbg->lock_nodes();
            _cerr("Summing unitig length bins...");

            std::vector<size_t> bin_sums(bins.size(), 0);

            for (auto it = cdbg->unodes_begin(); it != cdbg->unodes_end(); ++it) {
                auto seq_len = it->second->sequence.length();
                for (size_t bin_num = 0; bin_num < bins.size() - 1; bin_num++) {
                    if (seq_len >= bins[bin_num] && seq_len < bins[bin_num+1]) {
                        bin_sums[bin_num] += seq_len;
                        break;
                    }
                }
                if (seq_len > bins.back()) {
                    bins[bins.size() - 1] += seq_len;
                }
            }

            auto time_elapsed = std::chrono::system_clock::now() - time_start;
            _cerr("Finished summing unitig length bins. Elapsed time: " <<
                  std::chrono::duration<double>(time_elapsed).count());

            return bin_sums;
        }
    };


    class Writer : public reporting::MultiFileReporter {
    protected:

        std::shared_ptr<Graph> cdbg;
        cdbg::cDBGFormat format;

    public:

        Writer(std::shared_ptr<Graph> cdbg,
                   cdbg::cDBGFormat format,
                   const std::string& output_prefix)
            : MultiFileReporter(output_prefix,
                                "cDBGWriter[" + cdbg_format_repr(format) + "]"),
              cdbg(cdbg),
              format(format)
        {
            _cerr(this->THREAD_NAME << " reporting at COARSE interval.");

            this->msg_type_whitelist.insert(events::MSG_TIME_INTERVAL);
        }

        static std::shared_ptr<Writer> build(std::shared_ptr<Graph> cdbg,
                                             cdbg::cDBGFormat format,
                                             const std::string& output_prefix) {
            return std::make_shared<Writer>(cdbg, format, output_prefix);
        }

        virtual void handle_msg(std::shared_ptr<events::Event> event) {
            if (event->msg_type == events::MSG_TIME_INTERVAL) {
                auto _event = static_cast<events::TimeIntervalEvent*>(event.get());
                if (_event->level == events::TimeIntervalEvent::COARSE ||
                    _event->level == events::TimeIntervalEvent::END) {

                    std::ofstream& stream = this->next_stream(_event->t,
                                                              cdbg_format_repr(format));
                    std::string&   filename = this->current_filename();

                    _cerr(this->THREAD_NAME << ", t=" << _event->t <<
                          ": write cDBG to " << filename);
                    cdbg->write(stream, format);
                }
            }
        }
    };


};
}
}

#undef pdebug
#endif
