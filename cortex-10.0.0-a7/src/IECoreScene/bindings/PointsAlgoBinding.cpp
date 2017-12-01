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

#include "IECoreScene/PointsAlgo.h"
#include "IECorePython/RunTimeTypedBinding.h"

#include "PointsAlgoBinding.h"

using namespace boost;
using namespace boost::python;
using namespace IECorePython;
using namespace IECoreScene;

namespace
{

PointsPrimitivePtr mergePointsList( boost::python::list &pointsPrimitiveList )
{
	int numPointsPrimitives = boost::python::len( pointsPrimitiveList );
	std::vector<const PointsPrimitive *> pointsPrimitiveVec( numPointsPrimitives );

	for( int i = 0; i < numPointsPrimitives; ++i )
	{
		PointsPrimitivePtr ptr = boost::python::extract<PointsPrimitivePtr>( pointsPrimitiveList[i] );
		pointsPrimitiveVec[i] = ptr.get();
	}

	return PointsAlgo::mergePoints( pointsPrimitiveVec );
}

} // namepsace

namespace IECoreSceneModule
{

void bindPointsAlgo()
{
	object pointsAlgoModule( borrowed( PyImport_AddModule( "IECore.PointsAlgo" ) ) );
	scope().attr( "PointsAlgo" ) = pointsAlgoModule;

	scope pointsAlgoScope( pointsAlgoModule );

	def( "resamplePrimitiveVariable", &PointsAlgo::resamplePrimitiveVariable );
	def( "deletePoints", &PointsAlgo::deletePoints, arg_( "invert" ) = false);
	def( "mergePoints", &::mergePointsList );
}

} // namespace IECoreSceneModule
