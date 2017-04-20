##########################################################################
#
#  Copyright (c) 2017, Image Engine Design Inc. All rights reserved.
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
from IECore import *

class MeshAlgoTest( unittest.TestCase ) :

	def makeSingleTriangleMesh( self ):

		verticesPerFace = IntVectorData( [ 3 ] )
		vertexIds = IntVectorData( [ 0, 1, 2 ] )
		p = V3fVectorData( [ V3f( 0, 0, 0 ), V3f( 1, 0, 0 ), V3f( 0, 1, 0 ) ] )
		s = FloatVectorData( [ 0, 1, 0 ] )
		t = FloatVectorData( [ 0, 0, 1 ] )

		mesh = MeshPrimitive( verticesPerFace, vertexIds, "linear", p )
		mesh["s"] = PrimitiveVariable( PrimitiveVariable.Interpolation.FaceVarying, s )
		mesh["t"] = PrimitiveVariable( PrimitiveVariable.Interpolation.FaceVarying, t )

		mesh["foo_s"] = PrimitiveVariable( PrimitiveVariable.Interpolation.FaceVarying, FloatVectorData( [0, 0, 1] ) )
		mesh["foo_t"] = PrimitiveVariable( PrimitiveVariable.Interpolation.FaceVarying, FloatVectorData( [0, 1, 0] ) )

		prefData = V3fVectorData( [V3f( 0, 0, 0 ), V3f( 0, -1, 0 ), V3f( 1, 0, 0 )] )
		mesh["Pref"] = PrimitiveVariable( PrimitiveVariable.Interpolation.Vertex, prefData )

		return mesh

	def makeSingleBadUVTriangleMesh( self ) :

		verticesPerFace = IntVectorData( [3] )
		vertexIds = IntVectorData( [0, 1, 2] )
		p = V3fVectorData( [V3f( 0, 0, 0 ), V3f( 1, 0, 0 ), V3f( 0, 1, 0 )] )
		s = FloatVectorData( [0] )
		t = FloatVectorData( [0] )

		mesh = MeshPrimitive( verticesPerFace, vertexIds, "linear", p )
		mesh["s"] = PrimitiveVariable( PrimitiveVariable.Interpolation.Uniform, s )
		mesh["t"] = PrimitiveVariable( PrimitiveVariable.Interpolation.Uniform, t )

		return mesh

	def testSingleTriangleGeneratesCorrectTangents( self ) :
		triangle = self.makeSingleTriangleMesh()
		tangentPrimVar, bitangentPrimVar = MeshAlgo.calculateTangents( triangle )

		self.assertEqual(tangentPrimVar.interpolation, PrimitiveVariable.Interpolation.FaceVarying)
		self.assertEqual(bitangentPrimVar.interpolation, PrimitiveVariable.Interpolation.FaceVarying)

		tangents = V3fVectorData( tangentPrimVar.data )
		bitangent = V3fVectorData( bitangentPrimVar.data )

		self.assertEqual( len( tangents ), 3 )
		self.assertEqual( len( bitangent ), 3 )

		for t in tangents :
			self.assertAlmostEqual( t[0], 1.0 )
			self.assertAlmostEqual( t[1], 0.0 )
			self.assertAlmostEqual( t[2], 0.0 )

		for b in bitangent :
			self.assertAlmostEqual( b[0], 0.0 )
			self.assertAlmostEqual( b[1], 1.0 )
			self.assertAlmostEqual( b[2], 0.0 )


	def testJoinedUVEdges( self ) :

		mesh = ObjectReader( "test/IECore/data/cobFiles/twoTrianglesWithSharedUVs.cob" ).read()
		self.assert_( mesh.arePrimitiveVariablesValid() )

		tangentPrimVar, bitangentPrimVar = MeshAlgo.calculateTangents( mesh )

		self.assertEqual( tangentPrimVar.interpolation, PrimitiveVariable.Interpolation.FaceVarying )
		self.assertEqual( bitangentPrimVar.interpolation, PrimitiveVariable.Interpolation.FaceVarying )

		for v in tangentPrimVar.data :
			self.failUnless( v.equalWithAbsError( V3f( 1, 0, 0 ), 0.000001 ) )
		for v in bitangentPrimVar.data :
			self.failUnless( v.equalWithAbsError( V3f( 0, 0, -1 ), 0.000001 ) )

	def testSplitAndOpposedUVEdges( self ) :

		mesh = ObjectReader( "test/IECore/data/cobFiles/twoTrianglesWithSplitAndOpposedUVs.cob" ).read()

		tangentPrimVar, bitangentPrimVar = MeshAlgo.calculateTangents( mesh )

		self.assertEqual( tangentPrimVar.interpolation, PrimitiveVariable.Interpolation.FaceVarying )
		self.assertEqual( bitangentPrimVar.interpolation, PrimitiveVariable.Interpolation.FaceVarying )

		for v in tangentPrimVar.data[:3] :
			self.failUnless( v.equalWithAbsError( V3f( -1, 0, 0 ), 0.000001 ) )
		for v in tangentPrimVar.data[3:] :
			self.failUnless( v.equalWithAbsError( V3f( 1, 0, 0 ), 0.000001 ) )

		for v in bitangentPrimVar.data[:3] :
			self.failUnless( v.equalWithAbsError( V3f( 0, 0, 1 ), 0.000001 ) )
		for v in bitangentPrimVar.data[3:] :
			self.failUnless( v.equalWithAbsError( V3f( 0, 0, -1 ), 0.000001 ) )

	def testNonTriangulatedMeshRaisesException( self ):
		plane = MeshPrimitive.createPlane( Box2f( V2f( -0.1 ), V2f( 0.1 ) ) )
		self.assertRaises( RuntimeError, lambda : MeshAlgo.calculateTangents( plane ) )

	def testInvalidPositionPrimVarRaisesException( self ) :
		triangle = self.makeSingleTriangleMesh()
		self.assertRaises( RuntimeError, lambda : MeshAlgo.calculateTangents( triangle, position = "foo" ) )

	def testMissingUVsetPrimVarsRaisesException ( self ):
		triangle = self.makeSingleTriangleMesh()
		self.assertRaises( RuntimeError, lambda : MeshAlgo.calculateTangents( triangle, uvSet = "bar") )

	def testIncorrectUVPrimVarInterpolationRaisesException ( self ):
		triangle = self.makeSingleBadUVTriangleMesh()
		self.assertRaises( RuntimeError, lambda : MeshAlgo.calculateTangents( triangle ) )

	def testCanUseSecondUVSet( self ) :

		triangle = self.makeSingleTriangleMesh()
		uTangent, vTangent = MeshAlgo.calculateTangents( triangle , uvSet = "foo" )

		self.assertEqual( len( uTangent.data ), 3 )
		self.assertEqual( len( vTangent.data ), 3 )

		for v in uTangent.data :
			self.failUnless( v.equalWithAbsError( V3f( 0, 1, 0 ), 0.000001 ) )

		# really I'd expect the naive answer to the vTangent to be V3f( 1, 0, 0 )
		# but the code forces the triple of n, uT, vT to flip the direction of vT if we don't have a correctly handed set of basis vectors
		for v in vTangent.data :
			self.failUnless( v.equalWithAbsError( V3f( -1, 0, 0 ), 0.000001 ) )

	def testCanUsePref( self ) :

		triangle = self.makeSingleTriangleMesh()
		uTangent, vTangent = MeshAlgo.calculateTangents( triangle , position = "Pref")

		self.assertEqual( len( uTangent.data ), 3 )
		self.assertEqual( len( vTangent.data ), 3 )

		for v in uTangent.data :
			self.failUnless( v.equalWithAbsError( V3f( 0, -1, 0 ), 0.000001 ) )

		for v in vTangent.data :
			self.failUnless( v.equalWithAbsError( V3f( 1, 0, 0 ), 0.000001 ) )

if __name__ == "__main__":
	unittest.main()
