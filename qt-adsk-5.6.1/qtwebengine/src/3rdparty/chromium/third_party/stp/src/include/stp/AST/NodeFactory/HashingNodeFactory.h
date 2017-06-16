/********************************************************************
 * AUTHORS: Trevor Hansen
 *
 * BEGIN DATE: November, 2010
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

// A Node factory that only does structural hashing.
#ifndef HASHINGNODEFACTORY_H_
#define HASHINGNODEFACTORY_H_

#include "stp/AST/NodeFactory/NodeFactory.h"
#include "stp/AST/ASTKind.h"
namespace BEEV
{
class STPMgr;
}

class HashingNodeFactory : public NodeFactory
{
public:
  HashingNodeFactory(BEEV::STPMgr& bm_) : NodeFactory(bm_) {}

  virtual ~HashingNodeFactory();
  BEEV::ASTNode CreateNode(const BEEV::Kind kind,
                           const BEEV::ASTVec& back_children);
  BEEV::ASTNode CreateTerm(BEEV::Kind kind, unsigned int width,
                           const BEEV::ASTVec& children);

  virtual std::string getName() { return "hashing"; }
};

#endif /* HASHINGNODEFACTORY_H_ */
