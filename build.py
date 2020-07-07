#! /usr/bin/env python

import argparse
import functools
import glob
import hashlib
import json
import os
import operator
import md5
import multiprocessing
import subprocess
import shutil
import sys
import tarfile
import zipfile

__version = "1.5.0"

def __projects() :

	configFiles = glob.glob( "*/config.py" )
	return sorted( [ os.path.split( f )[0] for f in configFiles ] )

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

def __loadConfig( project ) :

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

	return config

def __substitute( config, variables, forDigest = False ) :

	if forDigest :
		# These shouldn't affect the output of the build, so
		# hold them constant when computing a digest.
		variables = variables.copy()
		variables.update( { "buildDir" : "{buildDir}", "jobs" : "{jobs}" } )

	def substituteWalk( o ) :

		if isinstance( o, dict ) :
			return { k : substituteWalk( v ) for k, v in o.items() }
		elif isinstance( o, list ) :
			return [ substituteWalk( x ) for x in o ]
		elif isinstance( o, tuple ) :
			return tuple( substituteWalk( x ) for x in o )
		elif isinstance( o, str ) :
			while True :
				s = o.format( **variables )
				if s == o :
					return s
				else :
					o = s
		else :
			return o

	return substituteWalk( config )

def __updateDigest( project, config ) :

	# This needs to account for everything that
	# `__buildProject()` is sensitive to.

	config["digest"].update( str( config["downloads"] ) )
	config["digest"].update( str( config.get( "license" ) ) )
	config["digest"].update( str( config.get( "environment" ) ) )
	config["digest"].update( str( config.get( "commands" ) ) )
	config["digest"].update( str( config.get( "symbolicLinks" ) ) )
	for e in config.get( "requiredEnvironment", [] ) :
		config["digest"].update( os.environ.get( e, "" ) )

	for patch in glob.glob( "{}/patches/*.patch".format( project ) ) :
		with open( patch ) as f :
			config["digest"].update( f.read() )

def __loadConfigs( variables ) :

	# Load configs

	configs = {}
	for project in __projects() :
		configs[project] = __loadConfig( project )

	# Walk dependency tree to compute digests and
	# apply substitutions.

	visited = set()
	def walk( project, configs ) :

		if project in visited :
			return

		projectConfig = configs[project]
		projectConfig["digest"] = hashlib.md5()

		# Visit dependencies to gather their public variables
		# and apply their digest to this project.

		projectVariables = variables.copy()
		for dependency in projectConfig.get( "dependencies", [] ) :
			walk( dependency, configs )
			projectConfig["digest"].update( configs[dependency]["digest"].hexdigest() )
			projectVariables.update( configs[dependency].get( "publicVariables", {} ) )

		# Apply substitutions and update digest.

		projectVariables.update( projectConfig.get( "publicVariables", {} ) )
		projectVariables.update( projectConfig.get( "variables", {} ) )

		projectConfig = __substitute( projectConfig, projectVariables, forDigest = True )
		__updateDigest( project, projectConfig )
		configs[project] = __substitute( projectConfig, projectVariables, forDigest = False )

		visited.add( project )

	for project in configs.keys() :
		walk( project, configs )

	return configs

def __preserveCurrentDirectory( f ) :

	@functools.wraps( f )
	def decorated( *args, **kw ) :
		d = os.getcwd()
		try :
			return f( *args, **kw )
		finally :
			os.chdir( d )

	return decorated

@__preserveCurrentDirectory
def __buildProject( project, config, buildDir ) :

	sys.stderr.write( "Building project {}\n".format( project ) )

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
		sys.stderr.write( "Linking {} to {}\n".format( link[0], link[1] ) )
		if os.path.exists( link[0] ) :
			os.remove( link[0] )
		os.symlink( link[1], link[0] )

def __checkConfigs( projects, configs ) :

	def walk( project, configs ) :

		if "url" not in configs[project] :
			sys.stderr.write( "{} is missing the \"url\" item\n".format( project ) )
			sys.exit( 1 )

		for e in configs[project].get( "requiredEnvironment", [] ) :
			if e not in os.environ :
				sys.stderr.write( "{} requires environment variable {}\n".format( project, e ) )
				sys.exit( 1 )

	for project in projects :
		walk( project, configs )

def __buildProjects( projects, configs, buildDir ) :

	digestsFilename = os.path.join( buildDir, ".digests" )
	if os.path.isfile( digestsFilename ) :
		with open( digestsFilename ) as digestsFile :
			digests = json.load( digestsFile )
	else :
		digests = {}

	built = set()
	def walk( project, configs, buildDir ) :

		if project in built :
			return

		for dependency in configs[project].get( "dependencies", [] ) :
			walk( dependency, configs, buildDir )

		if digests.get( project ) == configs[project]["digest"].hexdigest() :
			sys.stderr.write( "Project {} is up to date : skipping\n".format( project ) )
		else :
			__buildProject( project, configs[project], buildDir )
			digests[project] = configs[project]["digest"].hexdigest()
			with open( digestsFilename, "w" ) as digestsFile :
				json.dump( digests, digestsFile, indent = 4 )

		built.add( project )

	for project in projects :
		walk( project, configs, buildDir )

def __buildPackage( projects, configs, buildDir, package ) :

	sys.stderr.write( "Building package {}\n".format( package ) )

	visited = set()
	files = { "doc/licenses" }
	projectManifest = []

	def walk( project, configs, buildDir ) :

		if project in visited :
			return

		for dependency in configs[project].get( "dependencies", [] ) :
			walk( dependency, configs, buildDir )

		for pattern in configs[project].get( "manifest", [] ) :
			for f in glob.glob( os.path.join( buildDir, pattern ) ) :
				files.add( os.path.relpath( f, buildDir ) )

		m = {
			"name" : project,
			"url" : configs[project]["url"],
		}
		if "credit" in configs[project] :
			m["credit"] = configs[project]["credit"]
		if configs[project]["license"] :
			m["license"] = "./" + project
		projectManifest.append( m )

		visited.add( project )

	for project in projects :
		walk( project, configs, buildDir )

	projectManifest.sort( key = operator.itemgetter( "name" ) )
	with open( os.path.join( buildDir, "doc", "licenses", "manifest.json" ), "w" ) as file :
		json.dump( projectManifest, file, indent = 4 )

	rootName = os.path.basename( package ).replace( ".tar.gz", "" )
	with tarfile.open( package, "w:gz" ) as file :
		for m in files :
			file.add( os.path.join( buildDir, m ), arcname = os.path.join( rootName, m ) )

parser = argparse.ArgumentParser()

parser.add_argument(
	"--projects",
	choices = __projects(),
	nargs = "+",
	default = __projects(),
	help = "The projects to build."
)

parser.add_argument(
	"--buildDir",
	required = True,
	help = "The directory to put the builds in."
)

parser.add_argument(
	"--package",
	default = "gafferDependencies-{version}-{platform}.tar.gz",
	help = "The filename of the tarball package to create.",
)

parser.add_argument(
	"--jobs",
	type = int,
	default = multiprocessing.cpu_count(),
	help = "The number of build jobs to run in parallel. Defaults to cpu_count."
)

args = parser.parse_args()

variables = {
	"buildDir" : args.buildDir,
	"jobs" : args.jobs,
	"path" : os.environ["PATH"],
	"version" : __version,
	"platform" : "osx" if sys.platform == "darwin" else "linux",
	"sharedLibraryExtension" : ".dylib" if sys.platform == "darwin" else ".so",
}

configs = __loadConfigs( variables )

__checkConfigs( args.projects, configs )
__buildProjects( args.projects, configs, args.buildDir )

if args.package :
	__buildPackage( args.projects, configs, args.buildDir, args.package.format( **variables ) )
