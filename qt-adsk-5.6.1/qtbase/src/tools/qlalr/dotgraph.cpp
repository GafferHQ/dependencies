/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the utils of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "dotgraph.h"

#include "lalr.h"

#include <QtCore/qtextstream.h>

DotGraph::DotGraph(QTextStream &o):
  out (o)
{
}

void DotGraph::operator () (Automaton *aut)
{
  Grammar *g = aut->_M_grammar;

  out << "digraph {" << endl << endl;

  out << "subgraph Includes {" << endl;
  for (Automaton::IncludesGraph::iterator incl = Automaton::IncludesGraph::begin_nodes ();
       incl != Automaton::IncludesGraph::end_nodes (); ++incl)
    {
      for (Automaton::IncludesGraph::edge_iterator edge = incl->begin (); edge != incl->end (); ++edge)
        {
          out << "\t\"(" << aut->id (incl->data.state) << ", " << incl->data.nt << ")\"";
          out << "\t->\t";
          out << "\"(" << aut->id ((*edge)->data.state) << ", " << (*edge)->data.nt << ")\"\t";
          out << "[label=\"" << incl->data.state->follows [incl->data.nt] << "\"]";
          out << endl;
        }
    }
  out << "}" << endl << endl;


  out << "subgraph LRA {" << endl;
  //out << "node [shape=record];" << endl << endl;

  for (StatePointer q = aut->states.begin (); q != aut->states.end (); ++q)
    {
      int state = aut->id (q);

      out << "\t" << state << "\t[shape=record,label=\"{";

      out << "<0> State " << state;

      int index = 1;
      for (ItemPointer item = q->kernel.begin (); item != q->kernel.end (); ++item)
        out << "| <" << index++ << "> " << *item;

      out << "}\"]" << endl;

      for (Bundle::iterator a = q->bundle.begin (); a != q->bundle.end (); ++a)
        {
          const char *clr = g->isTerminal (a.key ()) ? "blue" : "red";
          out << "\t" << state << "\t->\t" << aut->id (*a) << "\t[color=\"" << clr << "\",label=\"" << a.key () << "\"]" << endl;
        }
      out << endl;
    }

  out << "}" << endl;
  out << endl << endl << "}" << endl;
}
