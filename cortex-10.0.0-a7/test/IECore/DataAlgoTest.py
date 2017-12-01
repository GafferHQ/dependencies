##########################################################################
#
#  Copyright (c) 2015, Image Engine Design Inc. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
#     * Neither the name of Image Engine Design nor the names of any
#       other contributors to this software may be used to endorse or
#       promote products derived from this software without specific prior
#       written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

import unittest
import math
import IECore

class DataAlgoTest( unittest.TestCase ) :

	def testGetGeometricInterpretation( self ) :

		self.assertEqual( IECore.GeometricData.Interpretation.Vector, IECore.getGeometricInterpretation( IECore.V3fVectorData( [], IECore.GeometricData.Interpretation.Vector ) ) )
		self.assertEqual( IECore.GeometricData.Interpretation.Normal, IECore.getGeometricInterpretation( IECore.V3fVectorData( [], IECore.GeometricData.Interpretation.Normal ) ) )
		self.assertEqual( IECore.GeometricData.Interpretation.Point, IECore.getGeometricInterpretation( IECore.V3fData( IECore.V3f( 1 ), IECore.GeometricData.Interpretation.Point ) ) )
		self.assertEqual( IECore.GeometricData.Interpretation.None, IECore.getGeometricInterpretation( IECore.V3fData( IECore.V3f( 1 ), IECore.GeometricData.Interpretation.None ) ) )
		self.assertEqual( IECore.GeometricData.Interpretation.None, IECore.getGeometricInterpretation( IECore.FloatData( 5 ) ) )
		self.assertEqual( IECore.GeometricData.Interpretation.None, IECore.getGeometricInterpretation( IECore.StringData( "foo" ) ) )

	def testSetGeometricInterpretation( self ) :

		v = IECore.V3fVectorData( [] )
		self.assertEqual( IECore.GeometricData.Interpretation.None, IECore.getGeometricInterpretation( v ) )
		IECore.setGeometricInterpretation( v, IECore.GeometricData.Interpretation.Vector )
		self.assertEqual( IECore.GeometricData.Interpretation.Vector, IECore.getGeometricInterpretation( v ) )
		IECore.setGeometricInterpretation( v, IECore.GeometricData.Interpretation.Normal )
		self.assertEqual( IECore.GeometricData.Interpretation.Normal, IECore.getGeometricInterpretation( v ) )

		v = IECore.V3fData( IECore.V3f( 0 ) )
		self.assertEqual( IECore.GeometricData.Interpretation.None, IECore.getGeometricInterpretation( v ) )
		IECore.setGeometricInterpretation( v, IECore.GeometricData.Interpretation.Point )
		self.assertEqual( IECore.GeometricData.Interpretation.Point, IECore.getGeometricInterpretation( v ) )
		IECore.setGeometricInterpretation( v, IECore.GeometricData.Interpretation.None )
		self.assertEqual( IECore.GeometricData.Interpretation.None, IECore.getGeometricInterpretation( v ) )


		#Setting the geometric interpretation of data that is not geometric is OK if you set it to None, but is otherwise an exception
		IECore.setGeometricInterpretation( IECore.FloatData( 5 ), IECore.GeometricData.Interpretation.None )
		IECore.setGeometricInterpretation( IECore.StringData( "foo" ), IECore.GeometricData.Interpretation.None )

		self.assertRaises( RuntimeError, IECore.setGeometricInterpretation, IECore.FloatData( 5 ), IECore.GeometricData.Interpretation.Normal )
		self.assertRaises( RuntimeError, IECore.setGeometricInterpretation, IECore.StringData( "foo" ), IECore.GeometricData.Interpretation.Point )



if __name__ == "__main__":
	unittest.main()
