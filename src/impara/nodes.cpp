#include <iostream>
#include <algorithm>

#include <util/i2string.h>

#include <path-symex/locs.h>

#include <symex/state.h>
#include "nodes.h"

//#define DEBUG

#ifdef DEBUG
#include <iostream>
#endif


/*******************************************************************\

Function: refine

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool nodet::refine(
  const namespacet &ns,
  merge_full_irept &merge,
  const exprt &other)
{  
  if(label.is_nil())
    label=other;
  else
  {    
    exprt old_label=label;
 
    impara_conjoin(other, label, ns);    
    simplify(label,ns);   
    
    merge(label);
    
    if(old_label==label)
    	return false;
    
    uncover_all();
  }
  
  if(label.is_false())
    ++coverings;
  
  return true;
}

/*******************************************************************\

Function: add_cover

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

unsigned node_reft::add_cover(node_reft node_ref) 
{
  get().cover.push_back(node_ref);

  nodet &other=*node_ref;
  
  ++other.coverings;

  #ifdef IMPARA_BIDIR
  
  if(!std::binary_search (other.covered_by.begin(), other.covered_by.end(),*this))
  {
  	other.covered_by.push_back(*this);
  	std::partial_sort(other.covered_by.begin(), 
  	                  other.covered_by.begin()+other.covered_by.size()-1,
  	                  other.covered_by.end());
  }
  #endif

  return other.uncover_all();
} 

/*******************************************************************\

Function: is_covered

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool nodet::is_covered() const 
{
  if(coverings > 0)
    return true;

  for(node_reft current=predecessor;
      !current.is_nil();
      --current) 
  {
    if(current->coverings > 0) 
      return true;
  }
  
  return false;
}



/*******************************************************************\

Function: node_reft::ancestors_same_class

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void node_reft::ancestors_same_class(std::vector<node_reft> &result)
{
  for(node_reft current=get().predecessor;
      !current.is_nil();
      --current) 
  {
    if(current.node_equiv_class==node_equiv_class) 
      result.push_back(current);
  }
}

/*******************************************************************\

Function: nearest_common_ancestor

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

node_reft node_reft::nearest_common_ancestor(const node_reft other) const
{
  node_reft current1 = *this;
  node_reft current2 = other;

  while(true) 
  {
    if(current1.is_nil() || current2.is_nil())
      break;
      
    const nodet &node1=*current1;
    const unsigned number1=node1.number;
    const nodet &node2=*current2;
    const unsigned number2=node2.number;

    if(number1==number2 && node1.has_label())
      break;
 
    if(number1 > number2) 
      --current1;
    else
      --current2;
  }

  return current1;
}

/*******************************************************************\

Function: uncover_all

  Inputs:

 Outputs: number of nodes uncovered

 Purpose:

\*******************************************************************/


unsigned nodet::uncover_all() 
{
  for(node_reft::listt::iterator 
      it =cover.begin(); 
      it!=cover.end(); 
      ++it) 
  {
    nodet &node = **it;
    --node.coverings;
    
    #ifdef IMPARA_BIDIR
    node_reft::listt new_covered_by;

    for(unsigned i=0; i<node.covered_by.size(); ++i)
    {
      node_reft &node_ref=node.covered_by[i];
      if(node_ref->number!=number)
      {
        new_covered_by.push_back(node_ref);
      }
    }    

    node.covered_by=new_covered_by;
    #endif
  }
	
	unsigned result=cover.size();

  cover.clear();
  
  return result;
}

/*******************************************************************\

Function: has_label

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/


bool nodet::has_label() const
{
  return label.is_not_nil();
}


/*******************************************************************\

Function: node_equiv_classt

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

node_equiv_classt::node_equiv_classt(const locst &locs, const global_vectort& global_vector)
{
}


/*******************************************************************\

Function: node_reft::labeled_successors

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

#ifdef IMPARA_BIDIR

void node_reft::labeled_successors(std::vector<node_reft> &dest,
                                   unsigned thread) const
{
  if(is_nil())
  {
    dest.clear();
  }
  else
  {
    bool change=true;

    std::vector<node_reft> frontier, new_frontier;

    frontier.push_back(*this);

    while(change)
    {
      change=false;

      new_frontier.clear();

      for(unsigned i=0; i<frontier.size(); ++i)
      {
        nodet &frontier_node=*frontier[i];

	if(frontier_node.has_label() 
        && frontier[i]!=*this
        && frontier_node.thread_nr==thread)
        {
          dest.push_back(frontier[i]);
        }
	else
        {
	        const std::vector<node_reft> 
	          &successors=frontier_node.successors;

          new_frontier.insert(new_frontier.end(), 
                          successors.begin(), 
                          successors.end());
          change=true;
        }
      }
      frontier.swap(new_frontier);
    }
  }
}


void node_reft::leaves(std::vector<node_reft> &dest) const
{
  if(is_nil())
  {
    dest.clear();
  }
  else
  {
    bool change=true;

    dest.push_back(*this);

    std::set<node_reft> seen;

    std::vector<node_reft> result, new_dest;

    while(change)
    {
      change=false;

      new_dest.clear();

			#ifdef DEBUG
			std::cout << "   Leaves ";
			#endif

      for(unsigned i=0; i<dest.size(); ++i)
      {

        if(seen.count(dest[i]))
        {       
          continue;
        }

				#ifdef DEBUG
				std::cout << dest[i]->number << " " << (dest[i]->is_covered() ? "C" : "U") << " ";
				#endif


        seen.insert(dest[i]);

        const std::vector<node_reft> 
          &successors=dest[i]->successors;

        node_reft::listt
          &covered_by=dest[i]->covered_by;

        if(covered_by.size()>0)
        {
        	// covered node itself also leaf
          new_dest.insert(new_dest.end(),
                          covered_by.begin(),
                          covered_by.end());
          change=true;
        } 
        else if(successors.size()>0)
        {
          new_dest.insert(new_dest.end(), 
                          successors.begin(), 
                          successors.end());
          change=true;
        }
        else
        {
          result.push_back(dest[i]);
        }
      }
      #ifdef DEBUG
      std::cout << std::endl;
      #endif
      dest.swap(new_dest);
    }

    dest.insert(dest.end(), result.begin(), result.end());
  }
}
#endif
