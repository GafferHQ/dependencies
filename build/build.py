#! /usr/bin/env python

import argparse
import glob
import os
import subprocess
import shutil
import sys

def __projects() :

	configFiles = glob.glob( "*/config.py" )
	return [ os.path.split( f )[0] for f in configFiles ]

def __decompress( archive ) :

	command = "tar -xvf {archive}".format( archive=archive )
	sys.stderr.write( command + "\n" )
	files = subprocess.check_output( command, stderr=subprocess.STDOUT, shell = True )
	files = [ f for f in files.split( "\n" ) if f ]
	files = [ f[2:] if f.startswith( "x " ) else f for f in files ]
	dirs = { f.split( "/" )[0] for f in files }
	assert( len( dirs ) ==  1 )
	return next( iter( dirs ) )

def __buildProject( project, buildDir ) :

	with open( project + "/config.py" ) as f :
		config =f.read()

	configContext = {
		"platform" : "osx" if sys.platform == "darwin" else "linux"
	}
	config = eval( config, configContext, configContext )

	archiveDir = project + "/archives"
	if not os.path.exists( archiveDir ) :
		os.makedirs( archiveDir )

	archives = []
	for download in config["downloads"] :

		archivePath = os.path.join( archiveDir, os.path.basename( download ) )
		archives.append( archivePath )

		if os.path.exists( archivePath ) :
			continue

		downloadCommand = "curl -L {0} > {1}".format( download, archivePath )
		sys.stderr.write( downloadCommand + "\n" )
		subprocess.check_call( downloadCommand, shell = True )

	workingDir = project + "/working"
	if os.path.exists( workingDir ) :
		shutil.rmtree( workingDir )
	os.makedirs( workingDir )
	os.chdir( workingDir )

	decompressedArchives = [ __decompress( "../../" + a ) for a in archives ]
	os.chdir( decompressedArchives[0] )

	if config["license"] is not None :
		shutil.copy( config["license"], os.path.join( buildDir, "doc/licenses", project ) )

	for patch in glob.glob( "../../patches/*.patch" ) :
		subprocess.check_call( "patch -p1 < {patch}".format( patch = patch ), shell = True )

	environment = os.environ.copy()
	for key, value in config.get( "environment", {} ).items() :
		environment[key] = value.replace( "$BUILD_DIR", buildDir )
	environment["BUILD_DIR"] = buildDir

	for command in config["commands"] :
		sys.stderr.write( command + "\n" )
		subprocess.check_call( command, shell = True, env = environment )

parser = argparse.ArgumentParser()

parser.add_argument(
	"--project",
	choices = __projects(),
	help = "The project to build."
)

parser.add_argument(
	"--buildDir",
	required = True,
	help = "The directory to put the builds in."
)

args = parser.parse_args()
__buildProject( args.project, args.buildDir )
