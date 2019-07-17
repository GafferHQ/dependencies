#! /usr/bin/env python

import argparse
import glob
import os
import multiprocessing
import subprocess
import shutil
import sys
import tarfile
import zipfile

def __projects() :

	configFiles = glob.glob( "*/config.py" )
	return [ os.path.split( f )[0] for f in configFiles ]

def __decompress( archive ) :

	if os.path.splitext( archive )[1] == ".zip" :
		with zipfile.ZipFile( archive ) as f :
			for info in f.infolist() :
				extracted = f.extract( info.filename )
				os.chmod( extracted, info.external_attr >> 16 )
			files = f.namelist()
	elif archive.endswith( ".tar.xz" ) :
		## \todo When we eventually move to Python 3, we can use
		# the `tarfile` module for this too.
		command = "tar -xvf {archive}".format( archive=archive )
		sys.stderr.write( command + "\n" )
		files = subprocess.check_output( command, stderr=subprocess.STDOUT, shell = True )
		files = [ f for f in files.split( "\n" ) if f ]
		files = [ f[2:] if f.startswith( "x " ) else f for f in files ]
	else :
		with tarfile.open( archive, "r:*" ) as f :
			f.extractall()
			files = f.getnames()

	dirs = { f.split( "/" )[0] for f in files }
	if len( dirs ) == 1 :
		# Well behaved archive with single top-level
		# directory.
		return next( iter( dirs ) )
	else :
		# Badly behaved archive
		return "./"

def __loadConfig( project, buildDir ) :

	# Load file. Really we want to use JSON to
	# enforce a "pure data" methodology, but JSON
	# doesn't allow comments so we use Python
	# instead. Because we use `eval` and don't expose
	# any modules, there's not much beyond JSON
	# syntax that we can use in practice.

	with open( project + "/config.py" ) as f :
		config =f.read()

	config = eval( config )

	# Apply platform-specific config overrides.

	platform = "platform:osx" if sys.platform == "darwin" else "platform:linux"
	platformOverrides = config.pop( platform, {} )
	for key, value in platformOverrides.items() :

		if isinstance( value, dict ) and key in config :
			config[key].update( value )
		else :
			config[key] = value

	# Apply variable substitutions.

	variables = config.get( "variables", {} ).copy()
	variables.update( {
		"buildDir" : buildDir,
		"jobs" : multiprocessing.cpu_count(),
	} )

	def __substitute( o ) :

		if isinstance( o, dict ) :
			return { k : __substitute( v ) for k, v in o.items() }
		elif isinstance( o, list ) :
			return [ __substitute( x ) for x in o ]
		elif isinstance( o, tuple ) :
			return tuple( __substitute( x ) for x in o )
		elif isinstance( o, str ) :
			while True :
				s = o.format( **variables )
				if s == o :
					return s
				else :
					o = s

	return __substitute( config )

def __buildProject( project, buildDir ) :

	config = __loadConfig( project, buildDir )

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
	os.chdir( config.get( "workingDir", decompressedArchives[0] ) )

	if config["license"] is not None :
		licenseDir = os.path.join( buildDir, "doc/licenses" )
		if not os.path.exists( licenseDir ) :
			os.makedirs( licenseDir )
		if os.path.isfile( config["license"] ) :
			shutil.copy( config["license"], os.path.join( licenseDir, project ) )
		else :
			shutil.copytree( config["license"], os.path.join( licenseDir, project ) )

	for patch in glob.glob( "../../patches/*.patch" ) :
		subprocess.check_call( "patch -p1 < {patch}".format( patch = patch ), shell = True )

	environment = os.environ.copy()
	for k, v in config.get( "environment", {} ).items() :
		environment[k] = os.path.expandvars( v )

	for command in config["commands"] :
		sys.stderr.write( command + "\n" )
		subprocess.check_call( command, shell = True, env = environment )

	for link in config.get( "symbolicLinks", [] ) :
		if os.path.exists( link[0] ) :
			os.remove( link[0] )
		os.symlink( link[1], link[0] )

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
