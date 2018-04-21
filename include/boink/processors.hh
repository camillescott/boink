/* consumer.hh -- dBG consumers
 *
 * Copyright (C) 2018 Camille Scott
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef CONSUMER_HH
#define CONSUMER_HH

#include <memory>
#include <iostream>

#include "oxli/read_parsers.hh"
#include "boink/parsing.hh"

using namespace oxli;
using namespace oxli:: read_parsers;

#define DEFAULT_OUTPUT_INTERVAL 10000

namespace boink {

template <class Derived,
          class ParserType = FastxReader>
class FileProcessor {

protected:

    uint64_t _n_reads;
    uint32_t _output_interval;
    uint32_t _output_counter;

public:

    FileProcessor(uint32_t output_interval=DEFAULT_OUTPUT_INTERVAL)
        : _output_interval(output_interval),
          _output_counter(0),
          _n_reads(0) {

    }

    uint64_t process(const string& left_filename,
                     const string& right_filename,
                     uint32_t min_length=0,
                     bool force_name_match=false) {
        SplitPairedReader<ParserType> reader(left_filename,
                                             right_filename,
                                             min_length,
                                             force_name_match);
        return process(reader);
    }

    uint64_t process(string const &filename) {
        ReadParserPtr<ParserType> parser = get_parser<ParserType>(filename);
        return process(parser);
    }

    uint64_t process(SplitPairedReader<ParserType>& reader) {
        ReadBundle bundle;
        
        while(!reader.is_complete()) {
            bundle = reader.next();
            derived().process_sequence(bundle);

            __sync_add_and_fetch( &_n_reads,
                                  bundle.has_left + bundle.has_right );
            __sync_add_and_fetch( &_output_counter,
                                  bundle.has_left + bundle.has_right);

            if (_output_counter == _output_interval) {
                _output_counter = 0;
                std::cerr << "processed " << _n_reads << " reads." << std::endl;
                derived().report();
            }  
        }

        return _n_reads;
    }

    uint64_t process(read_parsers::ReadParserPtr<ParserType>& parser) {
        Read read;

        // Iterate through the reads and consume their k-mers.
        while (!parser->is_complete( )) {
            try {
                read = parser->get_next_read( );
            } catch (NoMoreReadsAvailable) {
                break;
            }

            read.set_clean_seq();
            derived().process_sequence(read);

            __sync_add_and_fetch( &_n_reads, 1 );
            __sync_add_and_fetch( &_output_counter, 1 );

            if (_output_counter == _output_interval) {
                _output_counter = 0;
                std::cerr << "processed " << _n_reads << " reads." << std::endl;
                derived().report();
            }    
        }
        return _n_reads;
    }

    void process_sequence(ReadBundle& bundle) {
        if (bundle.has_left) {
            derived().process_sequence(bundle.left);
        }
        if (bundle.has_right) {
            derived().process_sequence(bundle.right);
        }
    }

    uint64_t n_reads() const {
        return _n_reads;
    }

private:

    FileProcessor() : _n_reads(0) {}

    friend Derived;

    Derived& derived() {
        return *static_cast<Derived*>(this);
    }

    const Derived& derived() const {
        return *static_cast<const Derived*>(this);
    }

};


template <class GraphType,
          class ParserType = FastxReader>
class FileConsumer : public FileProcessor<FileConsumer<GraphType, ParserType>,
                                          ParserType> {

protected:

    GraphType * graph;
    uint64_t _n_consumed;

    typedef FileProcessor<FileConsumer<GraphType, ParserType>,
                          ParserType> Base;

public:

    using Base::process_sequence;
    
    FileConsumer(GraphType * graph, uint32_t output_interval)
        : Base(output_interval),
          graph(graph), _n_consumed(0) {

    }

    void process_sequence(const Read& read) {
        auto this_n_consumed = graph->add_sequence(read.cleaned_seq);
        __sync_add_and_fetch( &_n_consumed, this_n_consumed );
    }

    void report() {
        std::cerr << "\t and " << _n_consumed << " new k-mers." << std::endl;
    }

    uint64_t n_consumed() const {
        return _n_consumed;
    }

};


template <class GraphType,
          class ParserType = FastxReader>
class DecisionNodeProcessor : public FileProcessor<DecisionNodeProcessor<GraphType, ParserType>,
                                                   ParserType> {

protected:
    typedef FileProcessor<DecisionNodeProcessor<GraphType, ParserType>,
                          ParserType> Base;
    StreamingCompactor<GraphType> * compactor;
    GraphType * graph;
    std::string _output_filename;
    std::ofstream _output_stream;

public:

    using Base::process_sequence;

    DecisionNodeProcessor(StreamingCompactor<GraphType> * compactor,
                          std::string& output_filename,
                          uint32_t output_interval)
        : Base(output_interval),
          compactor(compactor),
          graph(compactor->dbg),
          _output_filename(output_filename),
          _output_stream(_output_filename.c_str()) {

        _output_stream << "read_n, l_degree, r_degree, position, hash" << std::endl;

    }

    ~DecisionNodeProcessor() {
        _output_stream.close();
    }

    void process_sequence(const Read& read) {
        uint64_t n_new = graph->add_sequence(read.cleaned_seq);
        if (n_new > 0) {
            std::vector<uint32_t> decision_positions;
            HashVector decision_hashes;
            std::vector<NeighborBundle> decision_neighbors;
            std::set<hash_t> new_kmers;

            compactor->find_decision_kmers(read.cleaned_seq,
                                           decision_positions,
                                           decision_hashes,
                                           decision_neighbors);

            for (size_t i=0; i<decision_positions.size(); ++i) {
                _output_stream << this->n_reads() << ", "
                               << decision_neighbors[i].first.size() << ", "
                               << decision_neighbors[i].second.size() << ", "
                               << decision_positions[i] << ", "
                               << decision_hashes[i] << std::endl;
            }
        }
    }

    void report() {};

};


template <class GraphType,
          class ParserType = FastxReader>
class StreamingCompactorProcessor : 
    public FileProcessor<StreamingCompactorProcessor<GraphType, ParserType>,
                         ParserType> { // template class names like modern art

protected:

    StreamingCompactor<GraphType> * compactor;
    GraphType * graph;
    std::string _output_filename;
    std::ofstream _output_stream;

    typedef FileProcessor<StreamingCompactorProcessor<GraphType, ParserType>,
                          ParserType> Base;

public:

    using Base::process_sequence;

    StreamingCompactorProcessor(StreamingCompactor<GraphType> * compactor,
                                std::string& output_filename,
                                uint32_t output_interval)
        : Base(output_interval),
          compactor(compactor),
          graph(compactor->dbg),
          _output_filename(output_filename),
          _output_stream(_output_filename.c_str()) {

        _output_stream << compactor->cdbg.meta_counter.header() 
                       << ",n_dnodes,n_unodes,n_tags"
                       << ",n_updates,n_kmers,est_fp" << std::endl;

    }

    ~StreamingCompactorProcessor() {
        _output_stream.close();
    }

    void process_sequence(const Read& read) {
        try {
            compactor->update_sequence(read.cleaned_seq);
        } catch (BoinkException &e) {
            std::cerr << "WARNING: Bad sequence encountered: "
                      << read.cleaned_seq << ", exception was "
                      << e.what() << std::endl;
        }
    }

    void report() {
        std::cerr << "\tcurrently " << compactor->cdbg.n_decision_nodes()
                  << " d-nodes, " << compactor->cdbg.n_unitig_nodes()
                  << " u-nodes." << std::endl;
        _output_stream << this->n_reads() << ","
                       << compactor->cdbg.meta_counter
                       << "," << compactor->cdbg.n_decision_nodes()
                       << "," << compactor->cdbg.n_unitig_nodes()
                       << "," << compactor->cdbg.n_tags()
                       << "," << compactor->cdbg.n_updates()
                       << "," << compactor->dbg->n_unique()
                       << "," << compactor->dbg->estimated_fp()
                       << std::endl;
    }
};



template <class ShifterType,
          class ParserType = FastxReader>
class MinimizerProcessor : public FileProcessor<MinimizerProcessor<ShifterType, ParserType>,
                                                ParserType> {

protected:

    WKMinimizer<ShifterType> M;
    std::string _output_filename;
    std::ofstream _output_stream;

    typedef FileProcessor<MinimizerProcessor<ShifterType, ParserType>,
                                             ParserType> Base;
public:

    using Base::process_sequence;

    MinimizerProcessor(int32_t window_size,
                       uint16_t K,
                       const std::string& output_filename,
                       uint32_t output_interval)
        : Base(output_interval),
          M(window_size, K),
          _output_filename(output_filename),
          _output_stream(_output_filename.c_str()) {
    
    }

    ~MinimizerProcessor() {
        _output_stream.close();
    }

    void process_sequence(const Read& read) {
        std::vector<typename WKMinimizer<ShifterType>::value_type> minimizers;
        minimizers = M.get_minimizers(read.cleaned_seq);

        for (auto min : minimizers) {
            _output_stream << this->n_reads() << ","
                           << min.second << ","
                           << min.first << ","
                           << read.cleaned_seq.substr(min.second, M.K())
                           << std::endl;
        }
    }

    void report() {}
};

} //namespace boink
#endif
