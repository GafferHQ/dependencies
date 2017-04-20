//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2017, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "boost/python.hpp"

#include "IECore/MeshAlgo.h"
#include "IECorePython/MeshAlgoBinding.h"
#include "IECorePython/RunTimeTypedBinding.h"

using namespace boost::python;
using namespace IECore;

// Avoid cluttering the global namespace.
namespace
{

// Converts a std::pair instance to a Python tuple.
template<typename T1, typename T2>
struct StdPairToTuple
{
	static PyObject *convert( std::pair<T1, T2> const &p )
	{
		return boost::python::incref(
			boost::python::make_tuple( p.first, p.second ).ptr()
		);
	}

	static PyTypeObject const *get_pytype()
	{
		return &PyTuple_Type;
	}
};

// Helper for convenience.
template<typename T1, typename T2>
struct StdPairToTupleConverter
{
	StdPairToTupleConverter()
	{
		boost::python::to_python_converter<std::pair<T1, T2>, StdPairToTuple<T1, T2>, true //StdPairToTuple has get_pytype
										  >();
	}
};

} // namespace anonymous

namespace IECorePython
{

void bindMeshAlgo()
{
	object meshAlgoModule( borrowed( PyImport_AddModule( "IECore.MeshAlgo" ) ) );
	scope().attr( "MeshAlgo" ) = meshAlgoModule;

	scope meshAlgoScope( meshAlgoModule );

	StdPairToTupleConverter<IECore::PrimitiveVariable, IECore::PrimitiveVariable>();

	def( "calculateTangents", &MeshAlgo::calculateTangents, ( arg_( "uvSet" ) = "st", arg_( "orthoTangents" ) = true, arg_( "position" ) = "P" ) );
}

} // namespace IECorePython

