/*******************************************************************\

Module: Forward Path Search

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <cstdlib>
#include <algorithm>
#include <iostream>

#include <util/time_stopping.h>
#include <util/expr_util.h>
#include <util/simplify_expr.h>
#include <util/replace_expr.h>
#include <util/find_symbols.h>

#include <solvers/flattening/bv_pointers.h>

#include <solvers/sat/satcheck.h>
#include <solvers/smt2/smt2_dec.h>
#include <solvers/smt2/smt2_conv.h>

#include <langapi/language_util.h>

#include "impara_join.h"
#include "simple_checker.h"
#include "symex/loop_select.h"
#include "symex/build_goto_trace.h"

#include "interpolate/interpolator.h"

#include "interval_checker.h"

#include "impara_solver.h"

#include "impara_path_search.h"

//#define DEBUG
#define VERBOSE 0

/*******************************************************************\

Function: impara_path_searcht::covered

  Inputs: 

 Outputs: return true if state is covered

 Purpose:

\*******************************************************************/


bool impara_path_searcht::covered(statet& state)
{
  if(!do_cover)
    return false;

  if(close(state.node_ref))
    return true;

  if(!do_force_cover)
    return false;
 
  return force_cover(state, force_limit);
}

/*******************************************************************\

Function: impara_path_searcht::force_cover

  Inputs: k is the number of coverage candidates to look at

 Outputs: return true if state is covered

 Purpose:

\*******************************************************************/


bool impara_path_searcht::force_cover(statet &state,  
                                      unsigned k)
{
  if(k==0) return false;

  if(!state.node_ref->has_label())
    return false;

  // take the time
  absolute_timet force_start_time=current_time();

  #ifdef DEBUG
  std::cout << "impara_path_searcht::force_cover (begin)" << std::endl;
  #endif

  bool result = false;

  /* check covering by a recently explored node in the same class */
  node_equiv_classt 
   &node_equiv_class=state.node_ref.get_node_equiv_class();

  node_containert 
   &node_container=node_equiv_class.node_container;	
 
  nodet &node=*state.node_ref;
  std::size_t index=state.node_ref.get_index();
  unsigned attempts=0;

  for(int i=index-1; i>=0 && attempts<k; --i)
  {
    nodet &other=node_container[i];

    if(!other.is_cover_candidate(node))
      continue;

    ++attempts;

    node_reft other_ref(node_equiv_class, i);
    node_reft 
      ancestor=state.node_ref.nearest_common_ancestor(other_ref);

    if(ancestor.is_nil() || !ancestor->has_label())
      continue;

    bool loop=(other_ref==ancestor); 
  
    if(loop && cutpoints.count(state.pc())==0)
    {
      continue;
    }
  
  
    ++force_cover_checks_total;

    exprt pre=ancestor->get_label();
    exprt can=other.get_label();


    bool covered=true;
 
    if(do_propagation && can.id()==ID_and)
    {
 
      forall_operands(it, can)
      {
        exprt check=state.read(*it);

        if(check.is_false())
        {
          covered=false;
          continue;
        }
     
        if(do_show_vcc)
        {

          std::cout << "Force cover: " << state.node_ref->number 
            << " by " << other.number << "?" << std::endl;
          std::cout << "Force cover: nearest common ancestor " 
            << ancestor->number << std::endl;
        }  
        
        // check
        if(refine(
            state,               
            ancestor,    
            pre,     
            *it,  
            force_cover_solver_stats,
            false,
            loop))
        {    
          state.node_ref->refine(ns, merge, *it);
        } else
        {
          covered=false;
        }
      }
    }
    else
    {
      exprt check=state.read(can);

      if(check.is_false())
        continue;
   
      if(do_show_vcc)
      {

        std::cout << "Force cover: " << state.node_ref->number 
          << " by " << other.number << "?" << std::endl;
        std::cout << "Force cover: nearest common ancestor " 
          << ancestor->number << std::endl;
      }  
      
      // check
      if(refine(state,               
                ancestor,    
                pre,     
                can,  
                force_cover_solver_stats,
                false,
                loop))
      {    
        state.node_ref->refine(ns, merge, can);
      } else
      {
        covered=false;
      }
    }

    
    if(covered)
    {
      result=true;        
      ++force_cover_checks_ok;
      other_ref.add_cover(state.node_ref);
      exprt other_label=can;
      break;
    }
  }

  #ifdef DEBUG
  std::cout << "impara_path_searcht::force_cover (end)" << std::endl;
  #endif

  force_time+=current_time()-force_start_time;
  
  return result;
}


/*******************************************************************\

Function: impara_path_searcht::refine

  Inputs: 

 Outputs: return true iff a refinement has taken place

 Purpose:

\*******************************************************************/

bool impara_path_searcht::path_check(statet &state, 
                          node_reft ancestor,
                          exprt& assumption, 
                          exprt& conclusion,
                          simple_checkert& simple_checker,
                          impara_solver_statst& solver_stats,
                          bool build_trace,
                          bool loop)   
{
  bool result=true;

  // use the start label in simplification
  simple_checker.propagation.assume(assumption);

  if(do_simplify)
  {
    // take the time
    absolute_timet domain_start_time=current_time();
  
    decision_proceduret::resultt simple_result=
    simple_checker(assumption, conclusion);
		
    domain_time+=current_time()-domain_start_time;
		
    switch(simple_result)
    {
      case decision_proceduret::D_UNSATISFIABLE:
        if(do_show_vcc)
          std::cout << "Simple checker: UNSATISFIABLE" << std::endl;
      
        return true;
        break;
      case decision_proceduret::D_SATISFIABLE:
        if(do_show_vcc)
          std::cout << "Simple checker: UNSATISFIABLE" << std::endl;


        if(!build_trace && !(loop && do_strengthen))
          return false;
      break; 
      default:
        if(do_show_vcc)
          std::cout << "Simple checker: INCONCLUSIVE" << std::endl;
        break;
    }
  }

  if(do_simple_force && !build_trace)
    return false;

  // take the time
  solver_stats.log_begin();
  
  if(do_show_vcc)
  {
    status() << "Running SMT solver" << eom;
  }
  
  
  impara_solvert solver(ns);
  
  // SSA constraints
  solver.set_to(assumption, true);

  solver.set_to(simple_checker.propagation(conclusion), false);

  std::vector<literalt> guard_literals;
  std::vector<exprt> guards;

  state.history.convert(solver,
    ancestor,
    simple_checker.propagation,
    guard_literals,
    guards);
    
  unsigned nr_guards=guard_literals.size();

  std::vector<impara_solvert::contextt> guard_contexts(nr_guards);

  decision_proceduret::resultt dp_result;   

  // refinement loop
  while((dp_result=solver.dec_solve()) 
    == decision_proceduret::D_SATISFIABLE)
  {
    bool guard_added=false;
  
    for(unsigned i=0; i<nr_guards; ++i)
    {
      exprt eval_guard=solver.get(guards[i]);
 
      // guard excludes model
      if(eval_guard.is_false()) 
      {
        guard_contexts[i]=solver.new_context();      
        guard_added=true;
        guards[i]=true_exprt(); // do not add again 
        solver.set_to_context(
          guard_contexts[i], 
          literal_exprt(guard_literals[i]), true);
        
        // sufficient for UNSAT?
        break;
      }
    }

    // must be a valid model 
    if(!guard_added)
      break;
  }
  // stop time
  
  // doesn't work with SMT2
  solver_stats.log_end((satcheckt&)solver.satcheck);

  switch(dp_result)
  {
    case decision_proceduret::D_SATISFIABLE:
    
      if(do_show_vcc)
      {
        status() << "SMT solver: SAT" << eom;      
      }
    
      if(build_trace)
      {
        build_goto_trace(state, solver, error_trace);
      }
    
      if(loop && do_strengthen)
      {
        result=strengthen(state,
                   solver,
                   simple_checker,
                   ancestor, 
                   assumption, 
                   conclusion,
                   solver_stats);

      } 
      else
      {	  
        result=false;
      }      
      break;

      case decision_proceduret::D_UNSATISFIABLE:
      {
        if(do_show_vcc)
        {
          status() << "SMT solver: UNSAT" << eom;
        }

        state.history.get_core_steps(solver,
          ancestor,
          guard_contexts);          
        break;
      }
      default:
        throw "error from decision procedure";
  }
	
  return result;
}

bool impara_path_searcht::refine(statet &state, 
                          node_reft ancestor,
                          const exprt& start, 
                          const exprt& cond,
                          impara_solver_statst& solver_stats,
                          bool build_trace,
                          bool loop)   
{
  bool result=true;

  exprt assumption(state.history.rename(start,ancestor));
  
  exprt conclusion(state.history.rename(cond,state.node_ref));


  absolute_timet domain_start_time=current_time();
  simple_checkert simple_checker(locs, 
                                 ns, state.history, ancestor);
  domain_time+=current_time()-domain_start_time;

  if(do_show_vcc)
  {
    status() << " ========= refine (begin) =========== " << eom;

    state.history.output(ns, 
                         locs, 
                         simple_checker.propagation,
                         ancestor, 
                         assumption, 
                         conclusion, 
                         std::cout);
  }
  
  if(path_check(state, 
             ancestor,
             assumption, 
             conclusion,
             simple_checker,
             solver_stats,
             build_trace,
             loop))
  {

    if(!do_cover)
      result=true;
    else 
    {
      ++refinements;

      result=interpolate(state.history, 
                state.node_ref, 
                ancestor, 
                assumption, 
                conclusion);
    }
  } else
  {
    result=false;
  }

  if(do_show_vcc)
  {
    state.history.output(ns, 
                         locs, 
                         simple_checker.propagation,
                         ancestor, 
                         assumption, 
                         conclusion, 
                         std::cout);
    status() << " ========= refine (end) =========== " << eom;
  }


  // statistics
  return result;
}

/*******************************************************************\

Function: impara_path_searcht::interpolate

  Inputs: 

 Outputs: computes interpolants along path

 Purpose:

\*******************************************************************/

bool impara_path_searcht::interpolate(
              impara_step_reft history,
              node_reft node_ref,
              node_reft ancestor,
              const exprt& assumption, 
              const exprt& conclusion)
{
  pruned_node_ref.make_nil();

  interpolatort::interpolant_mapt itp_map;

  wp_interpolatort wpi(ns, options);
  wpi(history, node_ref, ancestor, assumption, conclusion, itp_map);

  for(interpolatort::interpolant_mapt::iterator
      m_it=itp_map.begin();
      m_it!=itp_map.end();
      m_it++)
  {

    node_reft node_ref=m_it->first;
    nodet &node=*node_ref;

    if(!node.has_label())
      continue;

    exprt label=m_it->second;

    //++cover_checks_total;

    if(node_ref != ancestor)
    { 
      absolute_timet close_start_time=current_time();
      bool imp=implies(node.get_label(),label, ns);
      close_time+=current_time()-close_start_time;

      if(!imp)
      {
        exprt old_label=node.get_label();

        merge(label);

       	node.refine(ns, merge, label);
      
        if(do_show_vcc)
        {
          status() << "Label@" << node.number << "\n"
                   << "     " 
                   << from_expr(ns, "     ", label) << "\n"
                   << "     " 
                   << from_expr(ns, "     ", old_label) << "\n" 
                   << "     => " 
                   << from_expr(ns, "     ", node.get_label()) 
                   << eom;
        } 
         
        if(do_refine_close)
        {
          if(pruned_node_ref.is_nil() && close(node_ref)) 
          {
            pruned_node_ref=node_ref;
          }
        }
      }
    } else {   

      if(do_show_vcc)
      {
        status() << "Keeping label@" << node.number << " "
                 << from_expr(ns, "", node.get_label()) << eom;
      }


      //++cover_checks_ok;
    }
  }
 
  if(do_show_vcc)
  {
    if(!pruned_node_ref.is_nil())
    {
      status() << "Refinement prunes ART at N" 
               << pruned_node_ref->number
               << eom;
    }
  }
  
  return true;
}


/*******************************************************************\

Function: impara_path_searcht::close

  Inputs: a node whose covering is to be computed

 Outputs: returns true if the node is covered by at least one other node

 Purpose: update covering information

\*******************************************************************/

bool impara_path_searcht::close(node_reft node_ref)
{
  bool result = false;

  // take the time
  absolute_timet close_start_time=current_time();

  nodet &node=*node_ref;
    
  if(!node.has_label())
  {
    result =false;
  } 
 
  else  
    
  if(node.is_covered()) 
  {
    result = true;
  } 
  
  else
  {
    node_equiv_classt &node_equiv_class=node_ref.get_node_equiv_class();
    node_containert &node_container=node_equiv_class.node_container;

    const exprt &this_label=node.get_label();
   
    unsigned node_index=node_ref.get_index();

    typedef 
      hash_map_cont<exprt, bool, irep_full_hash, irep_full_eq>
      implication_tablet;

    implication_tablet table;

    unsigned cover_checks=0;

    for(int i=node_index-1; i>=0 && cover_checks < cover_limit; --i)
    {
      nodet& other=node_container[i];

      if(!other.is_cover_candidate(node))
        continue;

      ++cover_checks_total;  

      const exprt &other_label=other.get_label();

      bool implied=true;

      if(other_label.id()==ID_and)
      {
        forall_operands(it, other_label)
        {
          const exprt &c=*it;

          implication_tablet::iterator iit=table.find(c);
          if(iit==table.end())
          {
            implied=implies(this_label, c, ns);
            table.insert(std::pair<exprt, bool>(c, implied));
          }
          else
          {
            implied=iit->second;
          }         
          if(!implied)
            break;
        }
      }
      else
      {
        implied=implies(this_label,other_label, ns);
      }
      
      ++cover_checks;
      

      // 'this label' needs to imply 'other label'
      if(implied)
      {
        node_reft other_ref(node_equiv_class,i);
	      other_ref.add_cover(node_ref);
        ++cover_checks_ok;
        result = true;
      }
    }
  }

  #ifdef DEBUG
  std::cout << "impara_path_searcht::close (end)" << std::endl;
  #endif

  close_time+=current_time()-close_start_time;
   
  return result;
}

/*******************************************************************\

Function: impara_path_searcht::ancestor_close

  Inputs: a node whose covering is to be computed

 Outputs: returns true if the node is covered by at least one other node

 Purpose: like close but only seeks to cover with ancestors
          dry_run == true ... only check for subsumption don't record the cover

\*******************************************************************/

bool impara_path_searcht::ancestor_close(node_reft node_ref,
                                         bool dry_run)
{
  bool result = false;

  // take the time
  absolute_timet close_start_time=current_time();

  nodet &node=*node_ref;
    
  if(!node.has_label())
  {
    result =false;
  } 
 
  else  
    
  if(node.is_covered()) 
  {
    result = true;
  } 
  
  else
  {
    std::vector<node_reft> ancestors;
  
    node_ref.ancestors_same_class(ancestors);
  
    const exprt &this_label=node.get_label();
   
    typedef 
      hash_map_cont<exprt, bool, irep_full_hash, irep_full_eq>
      implication_tablet;

    implication_tablet table;

    unsigned cover_checks=0;

    for(unsigned i=0; i<ancestors.size() && cover_checks < cover_limit; ++i)
    {
      nodet& other=*ancestors[i];

      if(!other.is_cover_candidate(node))
        continue;

      ++cover_checks_total;  

      const exprt &other_label=other.get_label();

      bool implied=true;

      if(other_label.id()==ID_and)
      {
        forall_operands(it, other_label)
        {
          const exprt &c=*it;

          implication_tablet::iterator iit=table.find(c);
          if(iit==table.end())
          {
            implied=implies(this_label, c, ns);
            table.insert(std::pair<exprt, bool>(c, implied));
          }
          else
          {
            implied=iit->second;
          }         
          if(!implied)
            break;
        }
      }
      else
      {
        implied=implies(this_label,other_label, ns);
      }
      
      ++cover_checks;
      

      // 'this label' needs to imply 'other label'
      if(implied)
      {
        node_reft other_ref(ancestors[i]);
	      
	      if(!dry_run)
	      {
  	      other_ref.add_cover(node_ref);
  	    }
  	    
        ++cover_checks_ok;
        result = true;
      }
    }
  }

  #ifdef DEBUG
  std::cout << "impara_path_searcht::close (end)" << std::endl;
  #endif

  close_time+=current_time()-close_start_time;
   
  return result;
}




