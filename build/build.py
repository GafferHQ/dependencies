#! /usr/bin/env python

import argparse
import glob
import os
import multiprocessing
import subprocess
import shutil
import sys
import re
import urllib

def __projects() :

	configFiles = glob.glob( "*/config.py" )
	return [ os.path.split( f )[0] for f in configFiles ]

def __decompress( archive ) :

	command = "tar -xvf {archive}".format( archive=archive )
	if sys.platform == "win32":
		command = "cmake -E tar xvf {archive}".format( archive=archive )

	sys.stderr.write( command + "\n" )
	files = subprocess.check_output( command, stderr=subprocess.STDOUT, shell = True )
	files = [ f for f in files.split( "\n" ) if f ]
	files = [ f[2:] if f.startswith( "x " ) else f for f in files ]
	dirs = { f.split( "/" )[0] for f in files if re.search( "warning:", f ) == None }
	assert( len( dirs ) ==  1 )
	return next( iter( dirs ) )

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

	config["platform"] = "platform:{}".format({ "darwin": "osx", "linux":"linux", "win32": "windows"}.get( sys.platform, "linux" ))
	platformOverrides = config.pop( config["platform"], {} )

	for key, value in platformOverrides.items() :

		if isinstance( value, dict ) and key in config :
			config[key].update( value )
		else :
			config[key] = value

	# Apply variable substitutions.

	variables = config.get( "variables", {} ).copy()
	cmake_generator = "\"NMake Makefiles JOM\"" if config["platform"] == "platform:windows" else "\"Unix Makefiles\""
	default_variables = {
		"buildDir" : buildDir,
		"jobs" : multiprocessing.cpu_count(),
		"cmakeGenerator" : cmake_generator,
		"cmakeBuildType": "Release"
	}
	missing_variables = { k:v for (k, v) in default_variables.items() if k not in variables }
	variables.update( missing_variables )

	if config["platform"] == "platform:windows":
		# make sure JOM is in the path
		path_variable = ""
		if "environment" in config:
			path_variable = os.path.expandvars(config["environment"].get("PATH", "%PATH%"))
			config["environment"].update( { "PATH": path_variable + ";%ROOT_DIR%\\winbuild\\jom" } )
		else:
			config["environment"] = { "PATH": "%PATH%;%ROOT_DIR%\\winbuild\\jom" }

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
				s = s.replace( "/", os.sep )
				s = re.sub( "mv\\s" , "move ", s ) if config["platform"] == "platform:windows" else s
				s = re.sub( "cp\\s" , "copy ", s ) if config["platform"] == "platform:windows" else s
				if s == o :
					return s
				else :
					o = s

	# changing "/" to "\\" on windows will cause errors with the urls
	original_downloads = config.get("downloads", [])
	config = __substitute( config )
	config.update( { "downloads": original_downloads } )
	return config

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

		sys.stderr.write( "Downloading {}".format( download ) + "\n" )
		urllib.urlretrieve( download, archivePath )

	workingDir = project + "/working"
	if os.path.exists( workingDir ) :
		shutil.rmtree( workingDir )
	os.makedirs( workingDir )
	os.chdir( workingDir )

	decompressedArchives = [ __decompress( "../../" + a ) for a in archives ]
	os.chdir( decompressedArchives[0] )

	if config.get( "license" ) is not None :
		shutil.copy( config["license"], os.path.join( buildDir, "doc/licenses", project ) )

	patch_command = "%ROOT_DIR%\\winbuild\\patch\\bin\\patch" if config["platform"] == "platform:windows" else "patch"
	for patch in glob.glob( "../../patches/{}/*.patch".format( config["platform"].lstrip( "platform:" ) ) ) :
		subprocess.check_call( "{patch_command} -p1 < {patch}".format( patch = patch, patch_command = patch_command ), shell = True )

	environment = os.environ.copy()
	environment.update( config.get( "environment", {} ) )

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
