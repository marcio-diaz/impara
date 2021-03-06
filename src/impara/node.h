/*******************************************************************\

Module: Unwound CFG Nodes

Author: Daniel Kroening, kroening@kroening.com
        Bjoern Wachter, bjoern.wachter@gmail.com

\*******************************************************************/

#ifndef CPROVER_IMPARA_NODE_H
#define CPROVER_IMPARA_NODE_H

#include <util/merge_irep.h>
#include <util/simplify_expr.h>
#include "node_ref.h"
#include "impara_join.h"

#include <interleaving/mpor/dependency_chain.h>

#include "symex/impara_history.h"

class nodet
{
public:

  nodet():
    number(unsigned(-1)),
    coverings(0),
    thread_nr(0),
    no_interleavings(0),
    label(nil_exprt())
  {
  }

  class node_reft predecessor;

  unsigned number; 
  unsigned coverings;
  unsigned thread_nr;
  unsigned no_interleavings;

  exprt label;

  dependency_chaint dc;

  // cover set of this node
  node_reft::listt cover; 

  impara_step_reft history;

  bool is_covered() const;
  
  inline bool is_cover_candidate(nodet &node) const
  {
    return has_label() && !is_covered() && node.dc < dc
	   && node.no_interleavings >= no_interleavings;
  }

  bool has_label() const;

  const exprt &get_label() { return label; }

  bool refine(const namespacet &ns,
              merge_full_irept &merge,
              const exprt &other);
 
  inline bool operator==(const nodet& other)
  {
    return number == other.number;
  }
  
protected:
  friend class node_reft;
  unsigned uncover_all();
};


#endif
