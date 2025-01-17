#ifndef ADIAR_BDD_BDD_POLICY_H
#define ADIAR_BDD_BDD_POLICY_H

#include <adiar/data.h>
#include <adiar/internal/tuple.h>

#include <adiar/bdd/bdd.h>

namespace adiar
{
  //////////////////////////////////////////////////////////////////////////////
  /// \brief Logic related to being a 'Binary' Decision Diagram.
  //////////////////////////////////////////////////////////////////////////////
  class bdd_policy
  {
  public:
    typedef bdd reduced_t;
    typedef __bdd unreduced_t;

  public:
    static inline ptr_t reduction_rule(const node_t &n)
    {
      if (n.low == n.high) { return n.low; }
      return n.uid;
    }

    static inline tuple reduction_rule_inv(const ptr_t &child)
    {
      return { child, child };
    }

  public:
    static inline void compute_cofactor(bool /* on_curr_level */,
                                        ptr_t & /* low */,
                                        ptr_t & /* high */)
    { /* do nothing */ }
  };
}

#endif // ADIAR_BDD_BDD_POLICY_H
