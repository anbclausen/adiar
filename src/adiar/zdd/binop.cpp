#include <adiar/zdd.h>
#include <adiar/zdd/zdd_policy.h>

#include <adiar/file_stream.h>

#include <adiar/internal/assert.h>
#include <adiar/internal/product_construction.h>
#include <adiar/internal/tuple.h>

namespace adiar
{
  bool can_right_shortcut_zdd(const bool_op &op, const ptr_t sink)
  {
    ptr_t sink_F = create_sink_ptr(false);
    ptr_t sink_T = create_sink_ptr(true);

    return // Does it shortcut on this level?
         op(sink_F, sink) == sink_F && op(sink_T,  sink) == sink_F
      // Does it shortcut on all other levels below?
      && op(sink_F, sink_F) == sink_F && op(sink_T,  sink_F) == sink_F;
  }

  bool can_left_shortcut_zdd(const bool_op &op, const ptr_t sink)
  {
    ptr_t sink_F = create_sink_ptr(false);
    ptr_t sink_T = create_sink_ptr(true);

    return // Does it shortcut on this level?
      op(sink, sink_F) == sink_F && op(sink, sink_T) == sink_F
      // Does it shortcut on all other levels below?
      && op(sink_F, sink_F) == sink_F && op(sink_F,  sink_T) == sink_F;
  }

  bool zdd_skippable(const bool_op &op, ptr_t high1, ptr_t high2)
  {
    return (is_sink(high1) && is_sink(high2)
            && op(high1, high2) == create_sink_ptr(false))
      || (is_sink(high1) && can_left_shortcut_zdd(op, high1))
      || (is_sink(high2) && can_right_shortcut_zdd(op, high2));
  }

  //////////////////////////////////////////////////////////////////////////////
  // ZDD product construction policy
  class zdd_prod_policy : public zdd_policy, public prod_mixed_level_merger
  {
  public:
    static __zdd resolve_same_file(const zdd &zdd_1, const zdd &/* zdd_2 */,
                                   const bool_op &op)
    {
      // Compute the results on all children.
      ptr_t op_F = op(create_sink_ptr(false), create_sink_ptr(false));
      ptr_t op_T = op(create_sink_ptr(true), create_sink_ptr(true));

      // Does it collapse to a sink?
      if (op_F == op_T) {
        return zdd_sink(value_of(op_F));
      }

      return zdd_1;
    }

  public:
    static __zdd resolve_sink_root(const node_t &v1, const zdd& zdd_1,
                                   const node_t &v2, const zdd& zdd_2,
                                   const bool_op &op)
    {
      ptr_t sink_F = create_sink_ptr(false);

      if (is_sink(v1) && is_sink(v2)) {
        ptr_t p = op(v1.uid, v2.uid);
        return zdd_sink(value_of(p));
      } else if (is_sink(v1)) {
        if (can_left_shortcut_zdd(op, v1.uid))  {
          // Shortcuts the left-most path to {Ø} and all others to Ø
          return zdd_sink(false);
        } else if (is_left_irrelevant(op, v1.uid) && is_left_irrelevant(op, sink_F)) {
          // Has no change to left-most path to {Ø} and neither any others
          return zdd_2;
        }
      } else { // if (is_sink(v2)) {
        if (can_right_shortcut_zdd(op, v2.uid)) {
          // Shortcuts the left-most path to {Ø} and all others to Ø
          return zdd_sink(false);
        } else if (is_right_irrelevant(op, v2.uid) && is_right_irrelevant(op, sink_F)) {
          // Has no change to left-most path to {Ø} and neither any others
          return zdd_1;
        }
      }
      return __zdd(); // return with no_file
    }

  public:
    static size_t left_leaves(const bool_op &op)
    {
      return !can_left_shortcut_zdd(op, create_sink_ptr(false)) +
             !can_left_shortcut_zdd(op, create_sink_ptr(true));
    }

    static size_t right_leaves(const bool_op &op)
    {
      return !can_right_shortcut_zdd(op, create_sink_ptr(false)) +
             !can_right_shortcut_zdd(op, create_sink_ptr(true));
    }

  private:
    static tuple __resolve_request(const bool_op &op, ptr_t r1, ptr_t r2)
    {
      if (is_sink(r1) && can_left_shortcut_zdd(op, r1)) {
        return { r1, create_sink_ptr(true) };
      } else if (is_sink(r2) && can_right_shortcut_zdd(op, r2)) {
        return { create_sink_ptr(true), r2 };
      } else {
        return { r1, r2 };
      }
    }

  public:
    static prod_rec resolve_request(const bool_op &op,
                                    ptr_t low1, ptr_t low2, ptr_t high1, ptr_t high2)
    {
      // Skip node, if it would be removed in the following Reduce
      if (zdd_skippable(op, high1, high2)) {
        return prod_rec_skipto { low1, low2 };
      } else {
        return prod_rec_output {
          __resolve_request(op, low1, low2),
          __resolve_request(op, high1, high2)
        };
      }
    }

    static constexpr bool no_skip = false;
  };

  //////////////////////////////////////////////////////////////////////////////
  __zdd zdd_binop(const zdd &A, const zdd &B, const bool_op &op)
  {
    return product_construction<zdd_prod_policy>(A, B, op);
  }
}
