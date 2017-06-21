/********************************************************************
 * AUTHORS: Trevor Hansen
 *
 * BEGIN DATE: March, 2010
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

#ifndef BBNodeManager_H_
#define BBNodeManager_H_

// We can bitblast either via ASTNodes, or via AIG nodes. The AIG nodes use less
// memory, and produce better CNFs. The ASTNodes are the
// traditional way of doing it.

#include "stp/STPManager/STPManager.h"

namespace BEEV
{
class ASTNode;
typedef std::vector<ASTNode> ASTVec;

// Called by the bitblaster. This returns ASTNodes after applying the
// CreateSimpForm(..) simplifications.
class BBNodeManagerASTNode
{
  ASTNode ASTTrue, ASTFalse;
  STPMgr* stp;

  // no copy, no assign.
  BBNodeManagerASTNode& operator=(const BBNodeManagerASTNode& other);
  BBNodeManagerASTNode(const BBNodeManagerASTNode& other);

public:
  BBNodeManagerASTNode(STPMgr* _stp)
  {
    stp = _stp;
    ASTTrue = stp->CreateNode(TRUE);
    ASTFalse = stp->CreateNode(FALSE);
  }

  ~BBNodeManagerASTNode() {}

  ASTNode getTrue() { return ASTTrue; }

  ASTNode getFalse() { return ASTFalse; }

  ASTNode CreateSymbol(const ASTNode& n, unsigned i)
  {
    if (n.GetType() == BOOLEAN_TYPE)
    {
      assert(i == 0);
      return n;
    }
    else
      return stp->CreateNode(BOOLEXTRACT, n, stp->CreateBVConst(32, i));
  }

  // CreateSimpForm removes IFF which aren't handled by the cnf converter.
  ASTNode CreateNode(Kind kind, vector<ASTNode>& children)
  {
    return stp->CreateSimpForm(kind, children);
  }

  ASTNode CreateNode(Kind kind, const ASTNode& child0)
  {
    return stp->CreateSimpForm(kind, child0);
  }

  ASTNode CreateNode(Kind kind, const ASTNode& child0, const ASTNode& child1)
  {
    return stp->CreateSimpForm(kind, child0, child1);
  }

  ASTNode CreateNode(Kind kind, const ASTNode& child0, const ASTNode& child1,
                     const ASTNode& child2)
  {
    return stp->CreateSimpForm(kind, child0, child1, child2);
  }
};
}

#endif /* BBNodeManager_H_ */
