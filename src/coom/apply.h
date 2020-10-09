#ifndef COOM_APPLY_H
#define COOM_APPLY_H

#include <tpie/tpie.h>
#include <tpie/file_stream.h>

#include <stdint.h>

#include "data.h"

namespace coom
{
  //////////////////////////////////////////////////////////////////////////////
  /// Some preset operators
  //////////////////////////////////////////////////////////////////////////////
  typedef std::function<ptr_t(ptr_t,ptr_t)> bool_op;

  const bool_op and_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return unflag(sink1 & sink2);
  };

  const bool_op nand_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return unflag(sink1 & sink2) ^ 2u;
  };

  const bool_op or_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return unflag(sink1 | sink2);
  };

  const bool_op nor_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return unflag(sink1 | sink2) ^ 2u;
  };

  const bool_op xor_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return create_sink_ptr(value_of(sink1) ^ value_of(sink2));
  };

  const bool_op implies_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return create_sink_ptr(value_of(sink1) ? value_of(sink2) : true);
  };

  const bool_op impliedby_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return create_sink_ptr(value_of(sink2) ? value_of(sink1) : true);
  };

  const bool_op equiv_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return create_sink_ptr(sink1 == sink2);
  };

  const bool_op diff_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return create_sink_ptr(value_of(sink1) && !value_of(sink2));
  };

  const bool_op less_op = [](ptr_t sink1, ptr_t sink2) -> ptr_t
  {
    return create_sink_ptr(!value_of(sink1) && value_of(sink2));
  };

  //////////////////////////////////////////////////////////////////////////////
  /// \brief Given two OBDDs creates one as per an operator.
  ///
  /// Creates the product construction of the two given OBDDs, which is then
  /// reduced into the minimal OBDD in the out_nodes.
  ///
  /// \param in_nodes_1 Nodes in reverse topological order of the first OBDD.
  /// \param in_meta_1  The meta stream related to in_nodes_1
  ///
  /// \param in_nodes_2 Nodes in reverse topological order of the second OBDD.
  /// \param in_meta_2  The meta stream related to in_nodes_2
  ///
  /// \param op Binary boolean operator to be applied.
  ///
  /// \param out_nodes The output stream to send the nodes in reverse topological
  ///                  order.
  /// \param out_meta  The output stream for the associated meta information
  //////////////////////////////////////////////////////////////////////////////
  void apply(tpie::file_stream<node_t> &in_nodes_1,
             tpie::file_stream<meta_t> &in_meta_1,
             tpie::file_stream<node_t> &in_nodes_2,
             tpie::file_stream<meta_t> &in_meta_2,
             const bool_op &op,
             tpie::file_stream<node_t> &out_nodes,
             tpie::file_stream<meta_t> &out_meta);

  //////////////////////////////////////////////////////////////////////////////
  /// \brief Obtain the unreduced intermediate result for combining two given
  ///        OBDDs by an operator.
  ///
  /// Does the same as Apply given a single coom::node_t output stream. The
  /// output in reduce_node_arcs and reduce_sink_arcs will have to go through
  /// coom::reduce, before one can use them further. This function also assumes
  /// that at least one of the two given OBDDs have internal nodes.
  ///
  /// The reason one would care to use this function is only to be able to
  /// either record statistics about the intermediate result or to be able to
  /// close one or more of the two input streams. That is, with the version
  /// above the C++ compiler isn't able to infer that the input streams may be
  /// dead variables (i.e. they can be closed) before one starts the next
  /// reduction. So, one can use this function to explicitly close the input
  /// streams and with that limit the concurrent memory usage.
  ///
  /// \param out_nodes_1 Nodes in reverse topological order of the first OBDD.
  /// \param out_nodes_2 Nodes in reverse topological order of the second OBDD.
  ///
  /// \param op Binary boolean operator to be applied.
  ///
  /// \param reduce_node_arcs The output stream to send the transposed node-arcs
  ///                         for the later reduce.
  /// \param reduce_sink_arcs The output stream to send the sink-arcs for the
  ///                         later reduce.
  //////////////////////////////////////////////////////////////////////////////
  void apply(tpie::file_stream<node_t> &in_nodes_1,
             tpie::file_stream<meta_t> &in_meta_1,
             tpie::file_stream<node_t> &in_nodes_2,
             tpie::file_stream<meta_t> &in_meta_2,
             const bool_op &op,
             tpie::file_stream<arc_t> &reduce_node_arcs,
             tpie::file_stream<arc_t> &reduce_sink_arcs,
             tpie::file_stream<meta_t> &reduce_meta);
}

#endif // COOM_APPLY_H
