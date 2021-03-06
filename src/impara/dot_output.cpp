/*******************************************************************\

Module: Graph Visualisation of Abstract Reachability Tree 

Author: Daniel Kroening, kroening@kroening.com
        Bjoern Wachter, bjoern.wachter@gmail.com

\*******************************************************************/

#include <sstream>
#include <iostream>

#include <util/expr_util.h>
#include <util/simplify_expr.h>

#include <path-symex/locs.h>


#include <symex/state.h>
#include <symex/propagation.h>
#include <nodes.h>

std::string thread2color(unsigned thread)
{

  switch(thread)
  {
    case 0:
      return "black";
    case 1:
      return "green";
    case 2:
      return "red";
    case 3:
      return "blue";
    case 4:
      return "darkgreen";
    case 5:
      return "darkred";
    case 6:
      return "darkblue";
    case 7:
      return "gold";
    case 8:
      return "deeppink";
    case 9:
      return "firebrick";
    case 10:
      return "magenta";
    case 11:
      return "navyblue";
    case 12:
      return "tomato";
    case 13:
      return "royalblue";
    case 14:
      return "salmon";
    case 15:
      return "springgreen";
    default: // if we have more threads visulation may become hopeless anyway
      return "gray";
  }
  return "gray";
}


void replace(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}


std::string line_wrap(const std::string& s, unsigned wrap)
{
  std::string result;

  int i=0;

  for(std::string::const_iterator 
      sit=s.begin();
      sit!=s.end();
      ++sit)
  {
    
    ++i;
    result+=*sit;
    if(i>0 && !(i%wrap))
      result+="\\n";
  }
  return result;
}




void global_vector2string(const global_vectort& global_vector, std::string& pc_string)
{
    pc_string="PC=(";

    for(unsigned 
        thr=0;
        thr<global_vector.size();
        thr++)
    {
      std::string pc;

      if(global_vector[thr].back().loc_number==(unsigned)-1)
      {
        pc="X";
      }
      else {
        for(unsigned i=0; i<global_vector[thr].size(); ++i)
	  pc+=(i>0 ? "::" : "" ) + std::to_string(global_vector[thr][i].loc_number);
      }

      pc_string+= (thr>0 ? ",":"") + pc;
    }

    pc_string+=")";
}

/*******************************************************************\

Function: path_searcht::dot_output

  Inputs: out stream

 Outputs: creates a dot file (graphviz) in stream out

 Purpose:

\*******************************************************************/


void nodest::dot_output(std::ostream& out, node_reft error_node)
{
  std::set<unsigned> visible;

  // collect the ancestors
  
  if(!error_node.is_nil())
  {
    for(node_reft node_ref=error_node; 
        !node_ref.is_nil(); 
        node_ref=node_ref->predecessor)
    {
      visible.insert(node_ref->number);
    }
  }
  
  dot_output(out, visible);
}

void nodest::dot_output(std::ostream& out, std::set<unsigned>& visible)
{

  std::map<unsigned, unsigned> cover;

	propagationt prop(ns);

  out << "digraph art {" << std::endl;

  // go through all nodes
  for(node_mapt::iterator 
      map_it = node_map.begin();
      map_it!= node_map.end();
      map_it++)
  {
    
    const global_vectort& global_vector=map_it->first;
    std::string pc_string;
      
    global_vector2string(global_vector,pc_string);

    node_equiv_classt& equiv_class=map_it->second;
    node_containert& nodes=equiv_class.node_container;

    for(node_containert::iterator
        node_it  = nodes.begin();
        node_it != nodes.end();
        node_it++)
    {
      // node
      nodet& node=*node_it;

      std::string fillcolor=node.is_covered() ? "\"palegreen\"" : "\"snow\"";

      int penwidth=1;     

      if(visible.count(node.number))
      {
	      fillcolor="yellow";
      }

      std::string shape="box";
      std::string arrow_shape="normal";



      // parent edge as a transition
      if(!node.predecessor.is_nil())
      {
        node_reft predecessor=node.predecessor;

        // * get the active thread
        unsigned thread=node.thread_nr;

        // * get the current instruction
        const loc_reft predecessor_pc = node.history->pc;
        const loct &loc=locs[predecessor_pc];
        const goto_programt::instructiont &instruction=*loc.target;

        out << predecessor->number << "->"<<node.number;
        
        std::string instruction_string;

        if(instruction.is_goto())
        {
        
          if(instruction.is_backwards_goto())
          {
            arrow_shape="vee";
          }
        
          if(instruction.guard.is_true())
          {
            instruction_string="";
          } else {

            loc_reft false_branch(predecessor_pc);
            false_branch.increase();
         
            if(global_vector[thread].back() == false_branch)
            {
              exprt cond = not_exprt(instruction.guard);
              simplify(cond,ns);
              instruction_string=from_expr(ns, "", cond);
            } else
            {
              exprt cond = instruction.guard;
              simplify(cond,ns);
              instruction_string=from_expr(ns, "", cond);
            }
          }
        } 
        else if(instruction.is_assert())
        {
          exprt cond = not_exprt(instruction.guard);
          simplify(cond,ns);
          instruction_string=from_expr(ns, "", cond);     
          penwidth=3;
          fillcolor="khaki";
        }
        else
        {
          instruction_string = as_string(ns, instruction);
        }

	replace(instruction_string, "\n", "");
	replace(instruction_string, "\"", "");

        out << "[weight=10,color="<<thread2color(thread)
            <<", arrowhead="<<arrow_shape 
            <<", penwidth="<< penwidth
            <<", label=\"T" << std::to_string(thread)
            << " "<< line_wrap(instruction_string,40) << "\"];" << std::endl;
                

        if(!node.cover.empty()) {

          out << node.number << "->";
          out << "{";

          // label the transition with the corresponding thread and command
          for(node_reft::listt::const_iterator
              node_it=node.cover.begin();
              node_it!=node.cover.end();
              ++node_it)
          {
            const nodet& other_node=**node_it;
            out << (node_it!=node.cover.begin() ? ";":"") << other_node.number;
          }

          out << "}";
            
          out << "[constraint=false,style=\"dashed\",dir=none,color=gray];" << std::endl;
        }
      }     

      // covering edge
   
   
      if(node.get_label().is_not_nil())
      {
        penwidth=3;
        shape="box3d";
      }
      else
        penwidth=1;
      
      out << node.number 
          << " [shape="<<shape
          <<",fillcolor="<<fillcolor
          <<",penwith="<<penwidth
          <<",style=\"filled\",color=grey,label=\"" 
          << pc_string 
          << " N" << node.number
          << " K" << node.no_interleavings;

      if(node.get_label().is_not_nil())
      {

        std::string label_text(from_expr(ns,"",node.get_label()));


	replace(label_text, "\n", "");
	replace(label_text, "\\\"", "");
	replace(label_text, "\"", "");

        if(node.get_label().is_true())
        {
          out << "\\nTrue";
        } else if ( !node.predecessor.is_nil() && node.predecessor->get_label() == node.get_label() && label_text.size()>40 )
        {
          out <<"\\n...";
        } else
        {
          out <<"\\n"<< line_wrap(label_text,40);  
        }
      }
     
      out << "\\n"+node.dc.pretty();

      out << "\"];"<<std::endl;
   
    }
  }
  
  out << "}";
}

