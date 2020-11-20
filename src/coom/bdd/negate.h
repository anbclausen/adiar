#ifndef COOM_NEGATE_H
#define COOM_NEGATE_H

#include <coom/data.h>

#include <coom/bdd/bdd.h>

namespace coom
{
  //////////////////////////////////////////////////////////////////////////////
  /// \brief Negate a given node-based OBDD.
  //////////////////////////////////////////////////////////////////////////////
  bdd bdd_not(const bdd &bdd);
  bdd bdd_not(bdd &&bdd);
}

#endif // COOM_NEGATE_H