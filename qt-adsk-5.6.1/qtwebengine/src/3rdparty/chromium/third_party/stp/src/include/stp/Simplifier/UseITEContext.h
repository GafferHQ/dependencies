/********************************************************************
 * AUTHORS: Trevor Hansen
 *
 * BEGIN DATE: Feb, 2011
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

/*
  *    Descend through ITEs keeping a stack of what must be true/false.
  *    For instance. In the following:
  *        (ite (or a b) (not (or a b)) c )
  *
  *       In the lhs of the ITE, we know that a or b are true, so, we can
  *rewrite it to:
  *       (ite (or a b) false c)
  *
 */

#ifndef UseITEContext_H_
#define UseITEContext_H_

#include "stp/AST/AST.h"
#include "stp/STPManager/STPManager.h"

namespace BEEV
{
class UseITEContext // not copyable
{
  NodeFactory* nf;
  RunTimes* runtimes;
  ASTNode ASTTrue, ASTFalse;

  void addToContext(const ASTNode& n, ASTNodeSet& context)
  {
    if (n.GetKind() == NOT && n[0].GetKind() == OR)
    {
      ASTVec flat = FlattenKind(OR, n[0].GetChildren());
      for (size_t i = 0; i < flat.size(); i++)
        context.insert(nf->CreateNode(NOT, flat[i]));
    }
    else if (n.GetKind() == AND)
    {
      ASTVec flat = FlattenKind(AND, n.GetChildren());
      context.insert(flat.begin(), flat.end());
    }
    else
      context.insert(n);
  }

  // Unfortunately there can be a lot of paths through a small formula.
  // So we limit how often each node is visited.

  ASTNode visit(const ASTNode& n, map<ASTNode, int>& visited,
                ASTNodeSet& visited_empty, ASTNodeSet& context)
  {
    if (n.isConstant())
      return n;

    if (context.size() == 0 && visited_empty.find(n) != visited_empty.end())
      return n;

    if (context.size() == 0)
      visited_empty.insert(n);

    if (context.find(n) != context.end())
      return ASTTrue;

    if (context.find(nf->CreateNode(NOT, n)) != context.end())
      return ASTFalse;

    if (n.isAtom())
      return n;

    // Hacks to stop it blowing out..
    {
      if (visited[n]++ > 10)
        return n;

      if (context.size() > 20)
        return n;
    }

    ASTVec new_children;

    if (n.GetKind() == ITE)
    {
      ASTNodeSet lhsContext(context), rhsContext(context);
      addToContext(n[0], lhsContext);
      addToContext(nf->CreateNode(NOT, n[0]), rhsContext);
      new_children.push_back(visit(n[0], visited, visited_empty, context));
      new_children.push_back(visit(n[1], visited, visited_empty, lhsContext));
      new_children.push_back(visit(n[2], visited, visited_empty, rhsContext));
    }
    else
    {
      for (size_t i = 0; i < n.GetChildren().size(); i++)
        new_children.push_back(visit(n[i], visited, visited_empty, context));
    }

    ASTNode result;
    if (new_children != n.GetChildren())
      if (n.GetType() == BOOLEAN_TYPE)
        result = nf->CreateNode(n.GetKind(), new_children);
      else
        result = nf->CreateArrayTerm(n.GetKind(), n.GetIndexWidth(),
                                     n.GetValueWidth(), new_children);
    else
      result = n;

    return result;
  }

public:
  ASTNode topLevel(const ASTNode& n)
  {
    runtimes->start(RunTimes::UseITEContext);
    map<ASTNode, int> visited;
    ASTNodeSet context;
    ASTNodeSet empty;
    ASTNode result = visit(n, visited, empty, context);
    runtimes->stop(RunTimes::UseITEContext);
    return result;
  }

  UseITEContext(STPMgr* bm)
  {
    runtimes = bm->GetRunTimes();
    nf = bm->defaultNodeFactory;
    ASTTrue = bm->ASTTrue;
    ASTFalse = bm->ASTFalse;
  }
};
}

#endif
