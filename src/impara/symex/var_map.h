/*******************************************************************\

Module: Variable Numbering

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#ifndef CPROVER_PATH_SYMEX_VAR_MAP_H
#define CPROVER_PATH_SYMEX_VAR_MAP_H

#include <ostream>

#include <util/namespace.h>
#include <util/type.h>

class impara_var_mapt
{
public:
  explicit impara_var_mapt(const namespacet &_ns):
    ns(_ns), shared_count(0), local_count(0)
  {
  }

  struct var_infot
  {
    struct kindt { 
      enum kind_enumt { SHARED, THREAD_LOCAL, PROCEDURE_LOCAL } kind_enum;
      
      kindt() : kind_enum(SHARED) {}
      kindt(const irep_idt &symbol, const namespacet &ns);
    
      inline bool is_shared() const 
      { 
        return kind_enum==SHARED; 
      }
      
      inline bool is_procedure_local() const 
      { 
        return kind_enum == PROCEDURE_LOCAL;
      }

      void output(std::ostream &out) const;
    } kind;
    
    inline bool is_shared() const { return kind.is_shared(); }
    
    // the variables are numbered
    unsigned number;

    // full_identifier=symbol+suffix
    irep_idt full_identifier, symbol, suffix;

    // the type of the identifier (struct member or array)
    typet type;
    
    var_infot():kind(), number(0)
    {
    }
    
    irep_idt ssa_identifier(unsigned thread, unsigned counter) const;

    symbol_exprt ssa_symbol(unsigned thread, unsigned counter) const
    {
      symbol_exprt s=symbol_exprt(ssa_identifier(thread, counter), type);
      s.set(ID_C_SSA_symbol, true);
      s.set(ID_C_full_identifier, full_identifier);
      return s;
    }

    void output(std::ostream &out) const;
  };
  
  typedef std::unordered_map<irep_idt, var_infot, irep_id_hash> id_mapt;
  id_mapt id_map;

  var_infot &operator()(
    const irep_idt &symbol,
    const irep_idt &suffix,
    const typet &type);
  
  var_infot& operator[]
    (const irep_idt &full_identifier);
  
  void init(var_infot &var_info);

  const namespacet &ns;

  std::vector<irep_idt> shared_id_vec;

protected:
  unsigned shared_count, local_count;
  var_infot dummy;
};

#endif
