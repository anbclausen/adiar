#include <adiar/zdd.h>

#include <adiar/data.h>

#include <adiar/file.h>
#include <adiar/file_stream.h>
#include <adiar/file_writer.h>

#include <adiar/internal/traverse.h>

namespace adiar
{
  template<typename visitor_t>
  class zdd_sat_label_writer_visitor
  {
    visitor_t visitor;

    bool has_elem = false;

    label_file lf;
    label_writer lw;

  public:
    zdd_sat_label_writer_visitor() : lw(lf) { }

    ptr_t visit(const node_t &n)
    {
      const ptr_t next_ptr = visitor.visit(n);

      if (next_ptr == n.high && (next_ptr != n.low || visitor_t::keep_dont_cares)) {
        lw << label_of(n);
      }

      return next_ptr;
    }

    void visit(const bool s)
    {
      visitor.visit(s);
      has_elem = s;
    }

    const std::optional<label_file> get_result() const
    {
      if (has_elem) { return lf; } else { return std::nullopt; }
    }
  };

  class zdd_satmin_visitor : public traverse_satmin_visitor
  {
  public:
    static constexpr bool keep_dont_cares = false;
  };

  std::optional<label_file> zdd_minelem(const zdd &A)
  {
    zdd_sat_label_writer_visitor<zdd_satmin_visitor> v;
    traverse(A, v);
    return v.get_result();
  }

  class zdd_satmax_visitor
  {
  public:
    static constexpr bool keep_dont_cares = true;

    inline ptr_t visit(const node_t &n) {
      adiar_debug(!is_sink(n.high) || value_of(n.high), "high sinks are never false");
      return n.high;
    }

    inline void visit(const bool /*s*/)
    { }
  };

  std::optional<label_file> zdd_maxelem(const zdd &A)
  {
    zdd_sat_label_writer_visitor<zdd_satmax_visitor> v;
    traverse(A, v);
    return v.get_result();
  }
}
