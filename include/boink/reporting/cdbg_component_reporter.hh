/* cdbg_history_reporter.hh -- async reporters
 *
 * Copyright (C) 2018 Camille Scott
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef BOINK_CDBG_COMPONENT_REPORTER_HH
#define BOINK_CDBG_COMPONENT_REPORTER_HH

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <prometheus/registry.h>

#include "boink/boink.hh"
#include "boink/cdbg/cdbg.hh"
#include "boink/cdbg/compactor.hh"
#include "boink/event_types.hh"
#include "boink/metrics.hh"
#include "boink/reporting/reporters.hh"
#include "boink/reporting/report_types.hh"

#include "sparsepp/spp.h"


namespace boink {
namespace reporting {

class cDBGComponentReporterMetrics {

private:

    prometheus::Family<prometheus::Summary>&   recompute_time_family;
    prometheus::Summary::Quantiles             recompute_time_quantiles;

    prometheus::Family<prometheus::Gauge>&     component_counts_family;
    
public:
    using Quantile = prometheus::detail::CKMSQuantiles::Quantile;

    prometheus::Summary&                       recompute_time;
    prometheus::Gauge&                         n_components;
    prometheus::Gauge&                         max_component_size;
    prometheus::Gauge&                         min_component_size;

    cDBGComponentReporterMetrics(std::shared_ptr<prometheus::Registry> registry)
        : recompute_time_family     (prometheus::BuildSummary()
                                                 .Name("boink_cdbg_components_compute_time_seconds")
                                                 .Register(*registry)),
          recompute_time_quantiles  {{.75, .1},
                                     {.5,  .1},
                                     {.25, .1}},
          recompute_time            (recompute_time_family.Add({{"time", "quantiles"}},
                                                               recompute_time_quantiles)),
          component_counts_family   (prometheus::BuildGauge()
                                                 .Name("boink_cdbg_components_current_total")
                                                 .Register(*registry)),
          n_components              (component_counts_family.Add({{"size", "all_components"}})),
          max_component_size        (component_counts_family.Add({{"size", "max_component"}})),
          min_component_size        (component_counts_family.Add({{"size", "min_component"}}))
    {
    }
};

#define ifmetrics(x) do { if (metrics != nullptr) { x; } } while (0)
template <class GraphType>
class cDBGComponentReporter : public SingleFileReporter {
private:

    std::shared_ptr<cdbg::cDBG<GraphType>>        cdbg;

    cdbg::id_t                                    component_id_counter;
    spp::sparse_hash_map<cdbg::id_t, uint64_t>    component_size_map;

    uint64_t                                      min_component;
    uint64_t                                      max_component;

    // how large of a sample to take from the component size distribution
    size_t                                        sample_size;
    metrics::ReservoirSample<size_t>              component_size_sample;

    std::unique_ptr<cDBGComponentReporterMetrics> metrics;

public:

    cDBGComponentReporter(std::shared_ptr<cdbg::cDBG<GraphType>> cdbg,
                          const std::string&                     filename,
                          size_t                                 sample_size = 10000,
                          std::shared_ptr<prometheus::Registry>  registry = nullptr)
        : SingleFileReporter       (filename, "cDBGComponentReporter"),
          cdbg                     (cdbg),
          component_id_counter     (0),
          min_component            (ULLONG_MAX),
          max_component            (0),
          sample_size              (sample_size),
          component_size_sample    (sample_size)
    {
        _cerr(this->THREAD_NAME << " reporting at MEDIUM interval.");
        this->msg_type_whitelist.insert(events::MSG_TIME_INTERVAL);
        _output_stream << "read_n,n_components,max_component,min_component,sample_size,component_size_sample" << std::endl;

        if (registry != nullptr) {
            metrics = make_unique<cDBGComponentReporterMetrics>(registry);
        } else {
            metrics = nullptr;
        }
    }

    virtual void handle_msg(std::shared_ptr<events::Event> event) {
         if (event->msg_type == events::MSG_TIME_INTERVAL) {
            auto _event = static_cast<events::TimeIntervalEvent*>(event.get());
            if (_event->level == events::TimeIntervalEvent::MEDIUM ||
                _event->level == events::TimeIntervalEvent::END) {
                
                this->recompute_components();
                _output_stream << _event->t << ","
                               << component_size_map.size() << ","
                               << max_component << ","
                               << min_component << ","
                               << component_size_sample.get_sample_size() << ","
                               << "\"" << component_size_sample.get_result() << "\""
                               << std::endl;
            }
        }       
    }

    void recompute_components() {
        auto time_start = std::chrono::system_clock::now();
        auto lock = cdbg->lock_nodes();

        spp::sparse_hash_set<cdbg::id_t> seen;
        component_size_map.clear();
        component_size_sample.clear();

        for (auto unitig_it = cdbg->unodes_begin(); unitig_it != cdbg->unodes_end(); unitig_it++) {
            auto root = unitig_it->second.get();
            if (!seen.count(root->node_id)) {
                auto component_nodes = cdbg->traverse_breadth_first(root);
                cdbg::id_t component_id = (root->component_id == NULL_ID) ? ++component_id_counter : root->component_id;
                for (auto node : component_nodes) {
                    node->component_id = component_id;
                    seen.insert(node->node_id);
                }
                component_size_map[component_id] = component_nodes.size();
                component_size_sample.sample(component_nodes.size());
                max_component = (component_nodes.size() > max_component) ? component_nodes.size() : max_component;
                min_component = (component_nodes.size() < min_component) ? component_nodes.size() : min_component;
            }
        }

        ifmetrics(metrics->n_components.Set(component_size_map.size()));
        ifmetrics(metrics->max_component_size.Set(max_component));
        ifmetrics(metrics->min_component_size.Set(min_component));

        auto time_elapsed = std::chrono::system_clock::now() - time_start;
        ifmetrics(metrics->recompute_time.Observe(std::chrono::duration<double>(time_elapsed).count()));
    }
};

}
}

#endif