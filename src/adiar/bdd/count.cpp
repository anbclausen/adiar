#ifndef ADIAR_COUNT_CPP
#define ADIAR_COUNT_CPP

#include "count.h"

#include <adiar/file_stream.h>
#include <adiar/priority_queue.h>
#include <adiar/reduce.h>

#include <adiar/assert.h>

namespace adiar
{
  //////////////////////////////////////////////////////////////////////////////
  // Data structures
  struct partial_sum
  {
    uid_t uid;
    uint64_t sum;
  };

  //////////////////////////////////////////////////////////////////////////////
  // Priority queue functions
  struct count_queue_lt
  {
    bool operator()(const partial_sum &a, const partial_sum &b)
    {
      return a.uid < b.uid;
    }
  };

  struct count_queue_label
  {
    label_t label_of(const partial_sum &s)
    {
      return adiar::label_of(s.uid);
    }
  };

  typedef node_priority_queue<partial_sum, count_queue_label, count_queue_lt> count_priority_queue_t;

  //////////////////////////////////////////////////////////////////////////////
  // Helper functions
  template<bool count_skipped_layers>
  inline void count_resolve_request(count_priority_queue_t &pq, uint64_t &result, const sink_pred &sink_pred,
                                    ptr_t child_to_resolve, label_t current_label, label_t max_label,
                                    uint64_t ingoing_sum = 1u)
  {
    if (ingoing_sum == 0) { return; }

    if (is_sink_ptr(child_to_resolve)) {
      if (sink_pred(child_to_resolve)) {
        result += ingoing_sum * (count_skipped_layers
                                 ? 1u << (max_label - current_label)
                                 : 1u);
      }
    } else {
      pq.push({ child_to_resolve,
                ingoing_sum * (count_skipped_layers
                               ? 1u << (label_of(child_to_resolve) - current_label - 1u)
                               : 1u) });
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  size_t bdd_nodecount(const bdd &bdd)
  {
    return nodecount(bdd.file);
  }

  size_t bdd_varcount(const bdd &bdd)
  {
    return varcount(bdd.file);
  }

  template<bool count_skipped_layers>
  inline uint64_t count(const bdd &bdd,
                        label_t min_label,
                        label_t max_label,
                        const sink_pred &sink_pred)
  {
    node_stream<> ns(bdd);

    count_priority_queue_t partial_sums;
    partial_sums.hook_meta_stream(bdd);

    // Take root out and put its children into the priority queue
    // or count them immediately if they are sinks
    node_t root = ns.pull();

    uint64_t skipped_layers_before_root = count_skipped_layers && min_label < label_of(root)
      ? 1u << (label_of(root) - min_label)
      : 1u;

    uint64_t result = 0u;

    count_resolve_request<count_skipped_layers>(partial_sums, result, sink_pred,
                                                root.low, label_of(root), max_label,
                                                skipped_layers_before_root);
    count_resolve_request<count_skipped_layers>(partial_sums, result, sink_pred,
                                                root.high, label_of(root), max_label,
                                                skipped_layers_before_root);

    // Take out the rest of the nodes and process them one by one
    while (ns.can_pull()) {
      node_t n = ns.pull();

      if (partial_sums.current_layer() != label_of(n)) {
        partial_sums.setup_next_layer();
      }

      // Sum all ingoing arcs
      uint64_t sum = 0u;
      while (partial_sums.can_pull() && partial_sums.top().uid == n.uid) {
        sum += partial_sums.pull().sum;
      }

      // Put children of the current node into the priority queue or count them if they are sinks
      count_resolve_request<count_skipped_layers>(partial_sums, result, sink_pred,
                                                  n.low, label_of(n), max_label, sum);
      count_resolve_request<count_skipped_layers>(partial_sums, result, sink_pred,
                                                  n.high, label_of(n), max_label, sum);
    }

    return result;
  }

  uint64_t bdd_pathcount(const bdd &bdd, const sink_pred &sink_pred)
  {
    if (is_sink(bdd)) {
      return 0u;
    }

    return count<false>(bdd, 0, MAX_LABEL, sink_pred);
  }

  uint64_t bdd_satcount(const bdd &bdd,
                        label_t minimum_label,
                        label_t maximum_label,
                        const sink_pred& sink_pred)
  {
    if (is_sink(bdd)) {
      if (is_sink(bdd, sink_pred)) {
        return 1u << (maximum_label - minimum_label);
      }
      return 0u;
    }

    adiar_debug(minimum_label <= min_label(bdd),
               "given minimum_label should be smaller than the present root label");

    adiar_debug(max_label(bdd) <= maximum_label,
               "given maximum_label should be greater than the largest label in obdd");

    return count<true>(bdd, minimum_label, maximum_label, sink_pred);
  }

  uint64_t bdd_satcount(const bdd& bdd, size_t varcount, const sink_pred& sink_pred)
  {
    if (is_sink(bdd)) {
      if (is_sink(bdd, sink_pred)) {
        return 1u << varcount;
      }
      return 0u;
    }

    label_t minimum_label = min_label(bdd);
    label_t maximum_label = (minimum_label + varcount) - 1u;

    adiar_debug(max_label(bdd) <= maximum_label,
               "the bdd spans more labels than the provided varcount suggests");

    return count<true>(bdd, minimum_label, maximum_label, sink_pred);
  }

  uint64_t bdd_satcount(const bdd& bdd, const sink_pred& sink_pred)
  {
    if (is_sink(bdd)) {
      return 0u;
    }

    return count<true>(bdd, min_label(bdd), max_label(bdd), sink_pred);
  }
}

#endif // ADIAR_COUNT_CPP