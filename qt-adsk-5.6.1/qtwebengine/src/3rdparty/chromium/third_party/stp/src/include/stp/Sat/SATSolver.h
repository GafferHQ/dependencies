/********************************************************************
 * AUTHORS: Trevor Hansen
 *
 * BEGIN DATE: Aug, 2010
 *
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#ifndef SATSOLVER_H_
#define SATSOLVER_H_

#include "minisat/mtl/Vec.h"
#include "minisat/core/SolverTypes.h"
#include <iostream>

// Don't let the defines escape outside.

namespace BEEV
{
class SATSolver
{
private:
  SATSolver(const SATSolver&);      // no copy
  void operator=(const SATSolver&); // no assign.

public:
  SATSolver() {}

  virtual ~SATSolver() {}

  class vec_literals : public Minisat::vec<Minisat::Lit>
  {
  };

  virtual bool addClause(
      const SATSolver::vec_literals& ps) = 0; // Add a clause to the solver.

  virtual bool addArray(int array_id, const SATSolver::vec_literals& i,
                        const SATSolver::vec_literals& v,
                        const Minisat::vec<Minisat::lbool>&,
                        const Minisat::vec<Minisat::lbool>&)
  {
    std::cerr << "Not implemented";
    exit(1);
  }

  virtual bool okay() const = 0; // FALSE means solver is in a conflicting state

  virtual bool solve() = 0; // Search without assumptions.

  typedef uint8_t lbool;

  static inline Minisat::Lit mkLit(uint32_t var, bool sign)
  {
    Minisat::Lit p;
    p.x = var + var + (int)sign;
    return p;
  }

  virtual uint8_t modelValue(uint32_t x) const = 0;

  virtual uint32_t newVar() = 0;

  virtual unsigned long nVars() = 0;

  virtual void printStats() = 0;

  virtual void setSeed(int i)
  {
    std::cerr << "Setting the random seen is not implemented for this solver"
              << std::endl;
    exit(1);
  }

  virtual void setVerbosity(int v) = 0;

  virtual lbool true_literal() = 0;
  virtual lbool false_literal() = 0;
  virtual lbool undef_literal() = 0;

  // The simplifying solvers shouldn't eliminate index / value variables.
  virtual void setFrozen(uint32_t x) {}

  virtual int nClauses()
  {
    std::cerr << "Not yet implemented.";
    exit(1);
  }

  virtual bool simplify()
  {
    std::cerr << "Not yet implemented.";
    exit(1);
  }
};
}
#endif
