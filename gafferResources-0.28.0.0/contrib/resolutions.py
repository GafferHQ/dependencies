## Rough and ready script to resize textures based on the actual
## physical area of the UDIM they belong to.

import math
import subprocess
import collections

import IECore

s = IECore.SceneCache( "/home/john/dev/gaffer/doc/examples/gafferBot/caches/gafferBot.scc", IECore.IndexedIO.OpenMode.Read )

udimAreas = collections.defaultdict( int )

def walk( c ) :

	for n in c.childNames() :

		walk( c.child( n ) )

	if not c.hasObject() :
		return

	o = c.readObject( 0 )
	if not isinstance( o, IECore.MeshPrimitive ) :
		return

	IECore.FaceAreaOp()( copyInput=False, input=o )

	assert( o["s"].interpolation == IECore.PrimitiveVariable.Interpolation.FaceVarying )
	assert( o["t"].interpolation == IECore.PrimitiveVariable.Interpolation.FaceVarying )
	assert( o["faceArea"].interpolation == IECore.PrimitiveVariable.Interpolation.Uniform )

	s = o["s"].data
	t = o["t"].data

	faceVaryingIndex = 0
	vertsPerFace = o.verticesPerFace
	for i in range( 0, len( vertsPerFace ) ) :
		tileU = int( math.floor( s[faceVaryingIndex] ) )
		tileV = int( math.floor( t[faceVaryingIndex] ) )
		udim = 1001 + 10 * abs( tileV ) + tileU
		udimAreas[udim] += o["faceArea"].data[i]
		faceVaryingIndex += vertsPerFace[i]

walk( s )

print " ".join( [str( k ) for k in sorted( udimAreas.keys() )] )

print udimAreas.items()

udimSizes = { k : math.sqrt( d ) for k, d in udimAreas.items() }

maxSize = max( udimSizes.values() )
print "MAX", maxSize
print "MIN", min( udimSizes.values() )

for udim, size in udimSizes.items() :

	resolution = 512 * size / maxSize
	resolution = 2 ** math.ceil( math.log( resolution, 2 ) )

	print udim, resolution
	command = "oiiotool %s%.4d.tx --resize %dx%d -o %s%.4d.tif" % (
		"/home/john/Desktop/gafferBot/textures/base/base_COL/", udim,
		resolution, resolution,
		"/tmp/test512/", udim
	)

	subprocess.check_call( command, shell = True )

	print command
