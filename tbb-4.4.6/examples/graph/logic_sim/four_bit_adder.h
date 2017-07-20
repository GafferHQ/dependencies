/*
    Copyright 2005-2016 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks. Threading Building Blocks is free software;
    you can redistribute it and/or modify it under the terms of the GNU General Public License
    version 2  as  published  by  the  Free Software Foundation.  Threading Building Blocks is
    distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See  the GNU General Public License for more details.   You should have received a copy of
    the  GNU General Public License along with Threading Building Blocks; if not, write to the
    Free Software Foundation, Inc.,  51 Franklin St,  Fifth Floor,  Boston,  MA 02110-1301 USA

    As a special exception,  you may use this file  as part of a free software library without
    restriction.  Specifically,  if other files instantiate templates  or use macros or inline
    functions from this file, or you compile this file and link it with other files to produce
    an executable,  this file does not by itself cause the resulting executable to be covered
    by the GNU General Public License. This exception does not however invalidate any other
    reasons why the executable file might be covered by the GNU General Public License.
*/

#ifndef __TBBexample_graph_logicsim_fba_H
#define __TBBexample_graph_logicsim_fba_H 1

#include "one_bit_adder.h"

typedef composite_node< tuple< signal_t, signal_t, signal_t, signal_t, signal_t, signal_t, signal_t, signal_t, signal_t >,
                      tuple< signal_t, signal_t, signal_t, signal_t, signal_t > > fba_base_type;

class four_bit_adder : public fba_base_type {
    graph& my_graph;
    std::vector<one_bit_adder> four_adders; 
public:
    four_bit_adder(graph& g) : fba_base_type(g), my_graph(g), four_adders(4, one_bit_adder(g)) {
        make_connections();
        set_up_composite();
    }
    four_bit_adder(const four_bit_adder& src) : 
        fba_base_type(src.my_graph), my_graph(src.my_graph), four_adders(4, one_bit_adder(src.my_graph)) 
    {
        make_connections();
        set_up_composite();
    }
    ~four_bit_adder() {}

private:
    void make_connections() {
        make_edge(output_port<1>(four_adders[0]), input_port<0>(four_adders[1]));
        make_edge(output_port<1>(four_adders[1]), input_port<0>(four_adders[2]));
        make_edge(output_port<1>(four_adders[2]), input_port<0>(four_adders[3]));
    }
    void set_up_composite() {

        fba_base_type::input_ports_type input_tuple(input_port<0>(four_adders[0]/*CI*/), input_port<1>(four_adders[0]), input_port<2>(four_adders[0]), input_port<1>(four_adders[1]), input_port<2>(four_adders[1]), input_port<1>(four_adders[2]), input_port<2>(four_adders[2]), input_port<1>(four_adders[3]), input_port<2>(four_adders[3])); 

       fba_base_type::output_ports_type output_tuple(output_port<0>(four_adders[0]), output_port<0>(four_adders[1]), output_port<0>(four_adders[2]), output_port<0>(four_adders[3]),output_port<1>(four_adders[3]/*CO*/));

        fba_base_type::set_external_ports(input_tuple, output_tuple);
    }
};

#endif /* __TBBexample_graph_logicsim_fba_H */
