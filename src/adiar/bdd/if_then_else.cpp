#include <adiar/bdd.h>

#include <adiar/file_stream.h>
#include <adiar/file_writer.h>

#include <adiar/internal/assert.h>
#include <adiar/internal/levelized_priority_queue.h>
#include <adiar/internal/tuple.h>
#include <adiar/internal/util.h>

#include <adiar/statistics.h>

namespace adiar
{
  //////////////////////////////////////////////////////////////////////////////
  // Struct to hold statistics
  stats_t::if_else_t stats_if_else;

  //////////////////////////////////////////////////////////////////////////////
  // Data structures
  //
  // To make the merge simpler we will make sure that 'data_1_x' and 'data_2_x'
  // are ordered based on the order of the BDDs are given, i.e. the elements of
  // the if-BDD would only every be forwarded in 'data_1_x'. The then-elements
  // are forwarded only in 'data_2_x' if the if-BDD is also forwarded. Finally,
  // the placement of the else-BDD on whether any element from the if-BDD or
  // then-BDD has been forwarded.

  struct ite_triple_1 : triple
  {
    ptr_t source;
  };

#ifndef NDEBUG
  struct ite_triple_1_lt : public std::binary_function<ite_triple_1, ite_triple_1, bool>
  {
    bool operator()(const ite_triple_1 &a, const ite_triple_1 &b)
    {
      return triple_fst_lt()(a,b)
        || (!triple_fst_lt()(b,a) && a.source < b.source);
    }
  };
#else
  typedef triple_fst_lt ite_triple_1_lt;
#endif

  template<template<typename, typename> typename sorter_template,
           template<typename, typename> typename priority_queue_template>
  using ite_priority_queue_1_t =
  levelized_node_priority_queue<ite_triple_1, triple_label, ite_triple_1_lt,
                                external_sorter, external_priority_queue,
                                3>;

  struct ite_triple_2 : ite_triple_1
  {
    ptr_t data_1_low;
    ptr_t data_1_high;
  };

#ifndef NDEBUG
  struct ite_triple_2_lt : public std::binary_function<ite_triple_2, ite_triple_2, bool>
  {
    bool operator()(const ite_triple_2 &a, const ite_triple_2 &b)
    {
      return triple_snd_lt()(a,b)
        || (!triple_snd_lt()(b,a) && a.source < b.source);
    }
  };
#else
  typedef triple_snd_lt ite_triple_2_lt;
#endif

  template<template<typename, typename> typename priority_queue_template>
  using ite_priority_queue_2_t = priority_queue_template<ite_triple_2, ite_triple_2_lt>;

  struct ite_triple_3 : ite_triple_2
  {
    ptr_t data_2_low;
    ptr_t data_2_high;
  };

#ifndef NDEBUG
  struct ite_triple_3_lt : public std::binary_function<ite_triple_3, ite_triple_3, bool>
  {
    bool operator()(const ite_triple_3 &a, const ite_triple_3 &b)
    {
      return triple_trd_lt()(a,b)
        || (!triple_trd_lt()(b,a) && a.source < b.source);
    }
  };
#else
  typedef triple_trd_lt ite_triple_3_lt;
#endif

  template<template<typename, typename> typename priority_queue_template>
  using ite_priority_queue_3_t = priority_queue_template<ite_triple_3, ite_triple_3_lt>;

  //////////////////////////////////////////////////////////////////////////////
  // Helper functions
  node_file __ite_zip_bdds(const bdd &bdd_if, const bdd &bdd_then, const bdd &bdd_else)
  {
    // TODO: What is the performance of '<<' rather than 'unsafe_push'? If there
    // is a major difference, then we may want to "inline" the '<<' with its
    // _canonical check here.

    // TODO: Add an approximation of maximum 1-level cut before returning.
    // It can be approximated as:
    // max(bdd_if.max_1level_cut, bdd_then.max_1level_cut + bdd_else.max_1level_cut)

    ptr_t root_then = NIL, root_else = NIL;

    node_file out_nodes;
    node_writer nw(out_nodes);

    // zip 'then' and 'else' cases
    node_stream<true> in_nodes_then(bdd_then);
    node_stream<true> in_nodes_else(bdd_else);

    while (in_nodes_then.can_pull() || in_nodes_else.can_pull()) {
      bool from_then = in_nodes_then.can_pull()
        && (!in_nodes_else.can_pull()
            || in_nodes_then.peek() > in_nodes_else.peek());

      node_t n = from_then ? in_nodes_then.pull() : in_nodes_else.pull();

      if (from_then && !in_nodes_then.can_pull()) { root_then = n.uid; }
      if (!from_then && !in_nodes_else.can_pull()) { root_else = n.uid; }

      nw << n;
    }

    // push all nodes from 'if' conditional and remap its sinks
    adiar_debug(!is_nil(root_then), "Did not obtain root from then stream");
    adiar_debug(!is_nil(root_else), "Did not obtain root from else stream");

    node_stream<true> in_nodes_if(bdd_if);

    while (in_nodes_if.can_pull()) {
      node_t n = in_nodes_if.pull();

      n.low = is_sink(n.low)
        ? (value_of(n.low) ? root_then : root_else)
        : n.low;

      n.high = is_sink(n.high)
        ? (value_of(n.high) ? root_then : root_else)
        : n.high;

      nw << n;
    }

    return out_nodes;
  }

  inline bool ite_must_forward(node_t v, ptr_t t, label_t out_label, ptr_t t_seek)
  {
    return
      // is it a node at this level?
      is_node(t) && label_of(t) == out_label
      // and we should be seeing it later
      && t_seek < t
      // and we haven't by accident just run into it anyway
      && v.uid != t;
  }

  inline void ite_init_request(node_stream<> &in_nodes, node_t &v, label_t out_label,
                               ptr_t &low, ptr_t &high)
  {
    if (label_of(v) == out_label) {
      low = v.low;
      high = v.high;

      if (in_nodes.can_pull()) { v = in_nodes.pull(); }
    } else {
      low = high = v.uid;
    }
  }

  template<typename pq_1_t>
  inline void __ite_resolve_request(pq_1_t &ite_pq_1,
                                  arc_writer &aw,
                                  ptr_t source, ptr_t r_if, ptr_t r_then, ptr_t r_else)
  {
    // Early shortcut an ite, if the sinks of both cases have collapsed to the
    // same anyway
    if (is_sink(r_then) && is_sink(r_else) &&
        value_of(r_then) == value_of(r_else)) {

      aw.unsafe_push_sink(arc_t { source, r_then });
      return;
    }
    // Remove irrelevant parts of a request to prune requests similar to
    // shortcutting the operator in bdd_apply.
    r_then = is_false(r_if) ? NIL : r_then;
    r_else = is_true(r_if)  ? NIL : r_else;

    if (is_sink(r_if) && is_sink(r_then)) {
      // => ~NIL => r_if is a sink with the 'true' value
      aw.unsafe_push_sink(arc_t { source, r_then });
    } else if (is_sink(r_if) && is_sink(r_else)) {
      // => ~NIL => r_if is a sink with the 'false' value
      aw.unsafe_push_sink(arc_t { source, r_else });
    } else {
      ite_pq_1.push({ r_if, r_then, r_else, source });
    }
  }

  template<typename pq_1_t, typename pq_2_t, typename pq_3_t>
  __bdd __bdd_ite(const bdd &bdd_if, const bdd &bdd_then, const bdd &bdd_else,
                  const size_t pq_1_memory, const size_t pq_2_memory, const size_t pq_3_memory,
                  const size_t max_pq_size)
  {
    // Now, at this point we will not defer to using the Apply, so we can take
    // up memory by opening the input streams and evaluating trivial
    // conditionals.
    node_stream<> in_nodes_if(bdd_if);
    node_t v_if = in_nodes_if.pull();

    if (is_sink(v_if)) {
      return value_of(v_if) ? bdd_then : bdd_else;
    }

    node_stream<> in_nodes_then(bdd_then);
    node_t v_then = in_nodes_then.pull();

    node_stream<> in_nodes_else(bdd_else);
    node_t v_else = in_nodes_else.pull();

    // If the levels of 'then' and 'else' are disjoint and the 'if' BDD is above
    // the two others, then we can merely zip the 'then' and 'else' BDDs. This
    // is only O((N1+N2+N3)/B) I/Os!
    if (max_label(bdd_if) < label_of(v_then) &&
        max_label(bdd_if) < label_of(v_else) &&
        disjoint_labels(bdd_then, bdd_else)) {
      return __ite_zip_bdds(bdd_if,bdd_then,bdd_else);
    }
    // From here on forward, we probably cannot circumvent actually having to do
    // the product construction.

    arc_file out_arcs;
    arc_writer aw(out_arcs);

    pq_1_t ite_pq_1({bdd_if, bdd_then, bdd_else}, pq_1_memory, max_pq_size);
    pq_2_t ite_pq_2(pq_2_memory, max_pq_size);
    pq_3_t ite_pq_3(pq_3_memory, max_pq_size);

    // Process root and create initial recursion requests
    label_t out_label = label_of(fst(v_if.uid, v_then.uid, v_else.uid));
    id_t out_id = 0;

    ptr_t low_if, low_then, low_else, high_if, high_then, high_else;
    ite_init_request(in_nodes_if, v_if, out_label, low_if, high_if);
    ite_init_request(in_nodes_then, v_then, out_label, low_then, high_then);
    ite_init_request(in_nodes_else, v_else, out_label, low_else, high_else);

    uid_t out_uid = create_node_uid(out_label, out_id++);
    __ite_resolve_request(ite_pq_1, aw, out_uid, low_if, low_then, low_else);
    __ite_resolve_request(ite_pq_1, aw, flag(out_uid), high_if, high_then, high_else);

    size_t max_1level_cut = 0;

    // Process all nodes in topological order of both BDDs
    while (!ite_pq_1.empty() || !ite_pq_2.empty() || !ite_pq_3.empty()) {
      if (ite_pq_1.empty_level() && ite_pq_2.empty() && ite_pq_3.empty()) {
        aw.unsafe_push(create_level_info(out_label, out_id));

        ite_pq_1.setup_next_level();
        out_label = ite_pq_1.current_level();
        out_id = 0;

        max_1level_cut = std::max(max_1level_cut, ite_pq_1.size());
      }

      ptr_t source, t_if, t_then, t_else;
      bool with_data_1 = false, with_data_2 = false;
      ptr_t data_1_low = NIL, data_1_high = NIL, data_2_low = NIL, data_2_high = NIL;

      // Merge requests from priority queues
      if (ite_pq_1.can_pull()
          && (ite_pq_2.empty() || fst(ite_pq_1.top()) < snd(ite_pq_2.top()))
          && (ite_pq_3.empty() || fst(ite_pq_1.top()) < trd(ite_pq_3.top()))) {
        ite_triple_1 r = ite_pq_1.top();
        ite_pq_1.pop();

        source = r.source;
        t_if = r.t1;
        t_then = r.t2;
        t_else = r.t3;
      } else if (!ite_pq_2.empty()
                 && (ite_pq_3.empty() || snd(ite_pq_2.top()) < trd(ite_pq_3.top()))) {
        ite_triple_2 r = ite_pq_2.top();
        ite_pq_2.pop();

        source = r.source;
        t_if = r.t1;
        t_then = r.t2;
        t_else = r.t3;

        with_data_1 = true;
        data_1_low = r.data_1_low;
        data_1_high = r.data_1_high;
      } else {
        ite_triple_3 r = ite_pq_3.top();
        ite_pq_3.pop();

        source = r.source;
        t_if = r.t1;
        t_then = r.t2;
        t_else = r.t3;

        with_data_1 = true;
        data_1_low = r.data_1_low;
        data_1_high = r.data_1_high;

        with_data_2 = true;
        data_2_low = r.data_2_low;
        data_2_high = r.data_2_high;
      }

      // Seek request partially in stream
      ptr_t t_fst = fst(t_if,t_then,t_else);
      ptr_t t_snd = snd(t_if,t_then,t_else);
      ptr_t t_trd = trd(t_if,t_then,t_else);

      ptr_t t_seek = with_data_2 ? t_trd
                   : with_data_1 ? t_snd
                                 : t_fst;

      while (v_if.uid < t_seek && in_nodes_if.can_pull()) {
        v_if = in_nodes_if.pull();
      }
      while (v_then.uid < t_seek && in_nodes_then.can_pull()) {
        v_then = in_nodes_then.pull();
      }
      while (v_else.uid < t_seek && in_nodes_else.can_pull()) {
        v_else = in_nodes_else.pull();
      }

      // Forward information across the level
      if (ite_must_forward(v_if, t_if, out_label, t_seek) ||
          ite_must_forward(v_then, t_then, out_label, t_seek) ||
          ite_must_forward(v_else, t_else, out_label, t_seek)) {
        // An element should be forwarded, if it was not already forwarded
        // (t_seek <= t_x), if it isn't the last one to seek (t_x < t_trd), and
        // if we actually are holding it.
        bool forward_if   = t_seek <= t_if   && t_if < t_trd   && v_if.uid == t_if;
        bool forward_then = t_seek == t_then && t_then < t_trd && v_then.uid == t_then;
        bool forward_else = t_seek == t_else && t_else < t_trd && v_else.uid == t_else;

        int number_of_elements_to_forward = ((int) forward_if)
                                          + ((int) forward_then)
                                          + ((int) forward_else);

        if (with_data_1 || number_of_elements_to_forward == 2) {
          adiar_debug(!with_data_1 || t_seek != t_fst,
                      "cannot have data and still seek the first element");
          adiar_debug(!(with_data_1 && (number_of_elements_to_forward == 2)),
                      "cannot have forwarded an element, hold two unforwarded items, and still need to forward for something");

          if (with_data_1) {
            if (t_if < t_seek || forward_else) {
              node_t v2 = forward_else ? v_else : v_then;
              data_2_low = v2.low;
              data_2_high = v2.high;
            } else { // if (forward_if || t_else < t_seek)
              data_2_low = data_1_low;
              data_2_high = data_1_high;

              node_t v1 = forward_if ? v_if : v_then;
              data_1_low = v1.low;
              data_1_high = v1.high;
            }
          } else {
            node_t v1 = forward_if   ? v_if   : v_then;
            node_t v2 = forward_else ? v_else : v_then;

            data_1_low = v1.low;
            data_1_high = v1.high;
            data_2_low = v2.low;
            data_2_high = v2.high;
          }

          ite_pq_3.push({ t_if, t_then, t_else, source, data_1_low, data_1_high, data_2_low, data_2_high });

          while (ite_pq_1.can_pull() && ite_pq_1.top().t1 == t_if
                                     && ite_pq_1.top().t2 == t_then
                                     && ite_pq_1.top().t3 == t_else) {
            source = ite_pq_1.pull().source;
            ite_pq_3.push({ t_if, t_then, t_else, source, data_1_low, data_1_high, data_2_low, data_2_high });
          }
        } else {
          // got no data and the stream only gave us a single item to forward.
          node_t v1 = forward_if   ? v_if
                    : forward_then ? v_then
                                   : v_else;

          ite_pq_2.push({ t_if, t_then, t_else, source, v1.low, v1.high });

          while (ite_pq_1.can_pull() && ite_pq_1.top().t1 == t_if
                                     && ite_pq_1.top().t2 == t_then
                                     && ite_pq_1.top().t3 == t_else) {
            source = ite_pq_1.pull().source;
            ite_pq_2.push({ t_if, t_then, t_else, source, v1.low, v1.high });
          }
        }
        continue;
      }

      // Resolve current node and recurse
      if (is_sink(t_if) || out_label < label_of(t_if)) {
        low_if = high_if = t_if;
      } else {
        low_if = t_if == v_if.uid ? v_if.low : data_1_low;
        high_if = t_if == v_if.uid ? v_if.high : data_1_high;
      }

      if (is_nil(t_then) || is_sink(t_then) || out_label < label_of(t_then)) {
        low_then = high_then = t_then;
      } else if (t_then == v_then.uid) {
        low_then = v_then.low;
        high_then = v_then.high;
      } else if (t_seek <= t_if) {
        low_then = data_1_low;
        high_then = data_1_high;
      } else {
        low_then = data_2_low;
        high_then = data_2_high;
      }

      if (is_nil(t_else) || is_sink(t_else) || out_label < label_of(t_else)) {
        low_else = high_else = t_else;
      } else if (t_else == v_else.uid) {
        low_else = v_else.low;
        high_else = v_else.high;
      } else if (t_seek <= t_if && t_seek <= t_then) {
        low_else = data_1_low;
        high_else = data_1_high;
      } else {
        low_else = data_2_low;
        high_else = data_2_high;
      }

      // Resolve request
      adiar_debug(out_id < MAX_ID, "Has run out of ids");
      out_uid = create_node_uid(out_label, out_id++);

      __ite_resolve_request(ite_pq_1, aw, out_uid, low_if, low_then, low_else);
      __ite_resolve_request(ite_pq_1, aw, flag(out_uid), high_if, high_then, high_else);

      // Output ingoing arcs
      while (true) {
        arc_t out_arc = { source, out_uid };
        aw.unsafe_push_node(out_arc);

        if (ite_pq_1.can_pull() && ite_pq_1.top().t1 == t_if
                                && ite_pq_1.top().t2 == t_then
                                && ite_pq_1.top().t3 == t_else) {
          source = ite_pq_1.pull().source;
        } else if (!ite_pq_2.empty() && ite_pq_2.top().t1 == t_if
                                     && ite_pq_2.top().t2 == t_then
                                     && ite_pq_2.top().t3 == t_else) {
          source = ite_pq_2.top().source;
          ite_pq_2.pop();
        } else if (!ite_pq_3.empty() && ite_pq_3.top().t1 == t_if
                                     && ite_pq_3.top().t2 == t_then
                                     && ite_pq_3.top().t3 == t_else) {
          source = ite_pq_3.top().source;
          ite_pq_3.pop();
        } else {
          break;
        }
      }
    }

    // Push the level of the very last iteration
    aw.unsafe_push(create_level_info(out_label, out_id));

    out_arcs._file_ptr->max_1level_cut = max_1level_cut;
    return out_arcs;
  }

  size_t __ite_max_cut_upper_bound(const decision_diagram &in_if,
                                   const decision_diagram &in_then,
                                   const decision_diagram &in_else)
  {
    const size_t if_size = in_if.file_ptr()->size();
    const size_t then_size = in_then.file_ptr()->size();
    const size_t else_size = in_else.file_ptr()->size();

    const bits_approximation if_bits(if_size);
    const bits_approximation then_bits(then_size);
    const bits_approximation else_bits(else_size);

    const bits_approximation bound_bits = (if_bits + 1) * (then_bits + 1 + 2) * (else_bits + 1 + 2)
                                          + (then_bits + 1) + (else_bits + 1) + 2;

    if(bound_bits.may_overflow()) {
      return std::numeric_limits<size_t>::max();
    } else {
      return (if_size + 1) * (then_size + 1 + 2) * (else_size + 1 + 2)
             + (then_size + 1) + (else_size + 1) + 2;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  __bdd bdd_ite(const bdd &bdd_if, const bdd &bdd_then, const bdd &bdd_else)
  {
    // There are multiple cases, where this boils down to an Apply rather than
    // an If-Then-Else. The bdd_apply uses tuples rather than triples and only
    // two priority queues, so it will run considerably faster.
    //
    // The translations into Apply can be found in Figure 1 of "Efficient
    // Implementation of a BDD Package" of Karl S. Brace, Richard L. Rudell, and
    // Randal E. Bryant.

    // Resolve being given the same underlying file in both cases
    if (bdd_then.file._file_ptr == bdd_else.file._file_ptr) {
      return bdd_then.negate == bdd_else.negate
        ? __bdd(bdd_then)
        : bdd_xnor(bdd_if, bdd_then);
    }

    // Resolve being given the same underlying file for conditional and a case
    if (bdd_if.file._file_ptr == bdd_then.file._file_ptr) {
      return bdd_if.negate == bdd_then.negate
        ? bdd_or(bdd_if, bdd_else)
        : bdd_and(bdd_not(bdd_if), bdd_else);
    } else if (bdd_if.file._file_ptr == bdd_else.file._file_ptr) {
      return bdd_if.negate == bdd_else.negate
        ? bdd_and(bdd_if, bdd_then)
        : bdd_imp(bdd_if, bdd_then);
    }

    // Resolve being given a sink in one of the cases
    if (is_sink(bdd_then)) {
      return bdd_apply(value_of(bdd_then) ? bdd_if : bdd_not(bdd_if),
                       bdd_else,
                       value_of(bdd_then) ? or_op : and_op);
    } else if (is_sink(bdd_else))  {
      return bdd_apply(bdd_if,
                       bdd_then,
                       value_of(bdd_else) ? imp_op : and_op);
    }

    // Compute amount of memory available for auxiliary data structures after
    // having opened all streams.
    //
    // We then may derive an upper bound on the size of auxiliary data
    // structures and check whether we can run them with a faster internal
    // memory variant.
    const tpie::memory_size_type aux_available_memory = memory::available()
      // Input streams
      - 3*node_stream<>::memory_usage()
      // Output stream
      - arc_writer::memory_usage();

    const size_t max_pq_size = __ite_max_cut_upper_bound(bdd_if, bdd_then, bdd_else);

    constexpr size_t data_structures_in_pq_1 =
      ite_priority_queue_1_t<internal_sorter, internal_priority_queue>::DATA_STRUCTURES;

    constexpr size_t data_structures_in_pq_2 =
      ite_priority_queue_2_t<internal_priority_queue>::DATA_STRUCTURES;

    constexpr size_t data_structures_in_pq_3 =
      ite_priority_queue_3_t<internal_priority_queue>::DATA_STRUCTURES;

    const size_t pq_1_internal_memory =
      (aux_available_memory / (data_structures_in_pq_1 + data_structures_in_pq_2 + data_structures_in_pq_3)) * data_structures_in_pq_1;

    const size_t pq_2_internal_memory =
      ((aux_available_memory - pq_1_internal_memory) / (data_structures_in_pq_2 + data_structures_in_pq_3)) * data_structures_in_pq_2;

    const size_t pq_3_internal_memory =
      aux_available_memory - pq_1_internal_memory - pq_2_internal_memory;

    const size_t pq_1_memory_fits =
      ite_priority_queue_1_t<internal_sorter, internal_priority_queue>::memory_fits(pq_1_internal_memory);

    const size_t pq_2_memory_fits =
      ite_priority_queue_2_t<internal_priority_queue>::memory_fits(pq_2_internal_memory);

    const size_t pq_3_memory_fits =
      ite_priority_queue_3_t<internal_priority_queue>::memory_fits(pq_3_internal_memory);

    // TODO: maximum 1-level cut is sufficient for pq_2 and pq_3!
    if(max_pq_size <= pq_1_memory_fits && max_pq_size <= pq_2_memory_fits && max_pq_size <= pq_3_memory_fits) {
#ifdef ADIAR_STATS
      stats_if_else.lpq_internal++;
#endif
      return __bdd_ite<ite_priority_queue_1_t<internal_sorter, internal_priority_queue>,
                       ite_priority_queue_2_t<internal_priority_queue>,
                       ite_priority_queue_3_t<internal_priority_queue>>
        (bdd_if, bdd_then, bdd_else, pq_1_internal_memory, pq_2_internal_memory, pq_3_internal_memory, max_pq_size);
    } else {
#ifdef ADIAR_STATS
      stats_if_else.lpq_external++;
#endif
      const size_t pq_1_memory = aux_available_memory / 3;
      const size_t pq_2_memory = pq_1_memory;
      const size_t pq_3_memory = pq_1_memory;

      return __bdd_ite<ite_priority_queue_1_t<external_sorter, external_priority_queue>,
                       ite_priority_queue_2_t<external_priority_queue>,
                       ite_priority_queue_3_t<external_priority_queue>>
        (bdd_if, bdd_then, bdd_else, pq_1_internal_memory, pq_2_memory, pq_3_memory, max_pq_size);
    }
  }
}
