#! /usr/bin/env python3

import argparse
import functools
import glob
import hashlib
import inspect
import json
import os
import operator
import multiprocessing
import pathlib
import subprocess
import shutil
import sys
import tarfile
from telnetlib import STATUS
import zipfile
from urllib.request import urlretrieve
import stat

__version = "9.1.0"

"""
Config file format
==================

Each project config consists of a `config.py` file, which must
evaluate to a single dictionary containing the config. This
dictionary specifies everything necessary to build the project.

Dictionary fields
-----------------

### Build

- dependencies : List of projects that must be built before this one.
- downloads : List containing the source archives to be downloaded.
- requiredEnvironment : List of environment variables which must be set
  externally before `build.py` is run.
- environment : Dictionary of environment variables to provide to the
  build.
- preCommands : List containing build commands to run immediately before `commands`.
- commands : List containing the build commands to be executed. String items will
  be run using a Python `subprocess`. Lambda items will be run as functions with the
  project's `config` passed as an argument. All project variables, including global
  variables, are included in `config["variables"]`.
- postCommands : List containing build commands to run immediately after `commands`.
- postMovePaths : Dictionary of the form { sourcePath : destination } specifying
  paths that should be copied after `postCommands` have run. Sources man be either
  files or directories and may contain standard glob matching patterns.Destinations
  should be directories, which will be created if needed.
- enabled : May be set to `False` to disable a project entirely. This is
  only expected to be useful in conjunction with platform overrides or
  variants.

### Packaging

- url : URL for the project website.
- license : The name of the license file within the source.
- credit : Any specific credit required to be given to the project.
- manifest : List of output files to include in the final package.
  May contain standard glob matching patterns.

### Variable substitution

Any configuration value may reference variables using Python's standard
`{variableName}` string substitution syntax. Variables may contain nested
references to other variables. Projects may define custom variables in
addition to the standard global variables :

- variables : Dictionary of variables for use in this config.
- publicVariables : Dictionary of variables for use in this config
  and in other configs which list this project as a dependency.

### Platform overrides

Configs may specify platform-specific overrides in a dictionary named
"platform:macos" or "platform:linux". Where a config setting is a dictionary,
overrides are merged in via `dict.update()`, otherwise they completely
replace the original value.

### Variants

Projects may specify a set of variants which are used to provide different
permutations of the build.

- variants : Optional list of variants for this project. If a project has
  variants, a `--variant:<ProjectName>` command line option is automatically
  generated, and the variant is automatically included in the final
  package name.
- variant:<VariantName> : A dictionary of overrides to apply to the config when
  building the specified variant for this project. These are applied in the
  same way as platform overrides.
- variant:<ProjectName>:<VariantName> : Dictionary of overrides to apply
  based on the current variant of another project specified by `ProjectName`.
"""

def __compilerRoot() :

	compiler = shutil.which( "g++" )
	binDir = os.path.dirname( compiler )
	return os.path.dirname( binDir )

def __projects() :

	configFiles = glob.glob( "*/config.py" )
	return sorted( [ os.path.split( f )[0] for f in configFiles ] )

def __decompress( archive, cleanup = False ) :

	if os.path.splitext( archive )[1] == ".zip" :
		with zipfile.ZipFile( archive ) as f :
			for info in f.infolist() :
				extracted = f.extract( info.filename )
				os.chmod( extracted, ( info.external_attr >> 16 ) | stat.S_IWUSR )
			files = f.namelist()
	else :
		with tarfile.open( archive, "r:*" ) as f :
			f.extractall()
			files = f.getnames()

	if cleanup :
		os.unlink( archive )

	dirs = { f.split( "/" )[0] for f in files }
	if len( dirs ) == 1 :
		# Well behaved archive with single top-level
		# directory.
		return next( iter( dirs ) )
	else :
		# Badly behaved archive
		return "./"

def __loadJSON( project ) :

	# Load file. Really we want to use JSON to
	# enforce a "pure data" methodology, but JSON
	# doesn't allow comments so we use Python
	# instead. Because we use `eval` and don't expose
	# any modules, there's not much beyond JSON
	# syntax that we can use in practice.

	with open( project + "/config.py" ) as f :
		config = f.read()

	return eval( config )

def __applyConfigOverrides( config, key ) :

	overrides = config.get( key, {} )
	for key, value in overrides.items() :

		if isinstance( value, dict ) and key in config :
			config[key].update( value )
		else :
			config[key] = value

def __substitute( config, variables, forDigest = False ) :

	if forDigest :
		# These shouldn't affect the output of the build, so
		# hold them constant when computing a digest.
		variables = variables.copy()
		variables.update( { "buildDir" : "{buildDir}", "jobs" : "{jobs}" } )

	def substituteWalk( o ) :

		if isinstance( o, dict ) :
			return { substituteWalk( k ) : substituteWalk( v ) for k, v in o.items() }
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

def __appendHash( hash, value ) :

	if isinstance( value, list ) :
		value = [ v for v in value if not callable( v ) ]
	hash.update( str( value ).encode( "utf-8" ) )

def __updateDigest( project, config ) :

	# This needs to account for everything that
	# `__buildProject()` is sensitive to.

	__appendHash( config["digest"], config["downloads"] )
	__appendHash( config["digest"], config.get( "license" ) )
	__appendHash( config["digest"], config.get( "environment" ) )
	__appendHash( config["digest"], config.get( "commands" ) )
	__appendHash( config["digest"], config.get( "symbolicLinks" ) )
	__appendHash( config["digest"], config.get( "postMovePaths" ) )
	for e in config.get( "requiredEnvironment", [] ) :
		__appendHash( config["digest"], os.environ.get( e, "" ) )

	for patch in glob.glob( "{}/patches/*.patch".format( project ) ) + glob.glob( "{}/patches/{}/*.patch".format( project, config["platform"] ) ) :
		with open( patch ) as f :
			__appendHash( config["digest"], f.read() )

def __loadConfigs( variables, variants ) :

	# Load configs and apply variants and platform overrides.

	configs = {}
	for project in __projects() :
		config = __loadJSON( project )
		config[ "platform" ] = variables[ "platform" ]
		if project in variants :
			__applyConfigOverrides( config, "variant:{}".format( variants[project] ) )
		for variantProject, variant in variants.items() :
			__applyConfigOverrides( config, "variant:{}:{}".format( variantProject, variant ) )
		__applyConfigOverrides( config, { "darwin": "platform:macos", "win32": "platform:windows" }.get( sys.platform, "platform:linux" ) )
		if config.get( "enabled", True ) :
			configs[project] = config

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
			__appendHash( projectConfig["digest"], configs[dependency]["digest"].hexdigest() )
			projectVariables.update( configs[dependency].get( "publicVariables", {} ) )

		# Apply substitutions and update digest.

		projectVariables.update( projectConfig.get( "publicVariables", {} ) )
		projectVariables.update( projectConfig.get( "variables", {} ) )
		projectConfig["variables"] = projectVariables

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
def __buildProject( project, config, buildDir, cleanup ) :

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

		sys.stderr.write( 'Retrieving URL: "{}" to "{}"'.format( download, archivePath ) )
		urlretrieve( download, archivePath )

	workingDir = project + "/working"
	if os.path.exists( workingDir ) :
		shutil.rmtree( workingDir )
	os.makedirs( workingDir )
	os.chdir( workingDir )
	fullWorkingDir = os.getcwd()

	decompressedArchives = [ __decompress( "../../" + a, cleanup ) for a in archives ]
	os.chdir( config.get( "workingDir", decompressedArchives[0] ) )

	if config["license"] is not None :
		licenseDir = os.path.join( buildDir, "doc/licenses" )
		licenseDest = os.path.join( licenseDir, project )
		if not os.path.exists( licenseDir ) :
			os.makedirs( licenseDir )
		if os.path.isfile( config["license"] ) :
			shutil.copy( config["license"], licenseDest )
		else :
			if os.path.exists( licenseDest ) :
				shutil.rmtree( licenseDest )
			shutil.copytree( config["license"], licenseDest )

	for patch in glob.glob( "../../patches/*.patch" ) + glob.glob( "../../patches/{}/*.patch".format( config["platform"] ) ) :
		# subprocess.check_call( "git apply --ignore-space-change --ignore-whitespace --whitespace=nowarn {patch}".format( patch = patch ), shell = True )
		subprocess.check_call( "patch -p1 < {patch}".format( patch = patch ), shell = True )

	environment = os.environ.copy()
	for k, v in config.get( "environment", {} ).items() :
		environment[k] = os.path.expandvars( v )

	for command in config.get( "preCommands", [] ) + config["commands"] + config.get( "postCommands", [] ) :
		if isinstance( command, str ) :
			sys.stderr.write( command + "\n" )
			subprocess.check_call( command, shell = True, env = environment )
		elif callable( command ) :
			command( config )

	moves = {}
	for src, dest in config.get( "postMovePaths", {} ).items() :
		destPath = pathlib.Path( dest )

		for f in glob.glob( src ) :
			path = pathlib.Path( f )
			if path.is_file() :
				moves[path] = destPath / path.name
			else :
				# We could use `pathlib.Path.replace()` with the directory,
				# but it will fail if the directory already exists. Instead
				# we want to merge our directory with the existing contents.
				for p in [ i for i in path.glob( "**/*" ) if i.is_file() ] :
					moves[p] = destPath / p.relative_to( path.parent )

	for src, dest in moves.items() :
		dest.parent.mkdir( parents = True, exist_ok = True )
		src.replace( dest )

	for link in config.get( "symbolicLinks", [] ) :
		sys.stderr.write( "Linking {} to {}\n".format( link[0], link[1] ) )
		if os.path.lexists( link[0] ) :
			os.remove( link[0] )
		os.symlink( link[1], link[0] )

	if cleanup :
		shutil.rmtree( fullWorkingDir )

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

def __buildProjects( projects, configs, buildDir, cleanup ) :

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
			__buildProject( project, configs[project], buildDir, cleanup )
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

	if sys.platform == "win32" :
		rootName = os.path.basename( package ).replace( ".zip", "" )
		buildDirLength = len( buildDir )
		with zipfile.ZipFile( package, "w", zipfile.ZIP_DEFLATED ) as file:
			for m in files :
				path = os.path.join( buildDir, m )
				if os.path.isfile( path ) :
					file.write( os.path.join( buildDir, m ), arcname = os.path.join( rootName, m ) )
				elif os.path.isdir( path ) :
					for root, dirs, files in os.walk( path ):
						for f in files:
							fullPath = os.path.join( root, f )
							relativePath = fullPath[ buildDirLength : ].lstrip( "\\" )
							file.write( fullPath, arcname = os.path.join( rootName, relativePath ) )
	else :
		rootName = os.path.basename( package ).replace( ".tar.gz", "" )
		with tarfile.open( package, "w:gz", compresslevel = 6 ) as file :
			for m in files :
				file.add( os.path.join( buildDir, m ), arcname = os.path.join( rootName, m ) )

parser = argparse.ArgumentParser()

parser.add_argument(
	"--projects",
	choices = __projects(),
	nargs = "+",
	default = None,
	help = "The projects to build."
)

parser.add_argument(
	"--buildDir",
	default = "gafferDependencies-{version}{variants}-{platform}",
	help = "The directory to put the builds in."
)

parser.add_argument(
	"--package",
	default = "gafferDependencies-{version}{variants}-{platform}" + ( ".zip" if sys.platform == "win32" else ".tar.gz" ),
	help = "The filename of the tarball package to create.",
)

parser.add_argument(
	"--jobs",
	type = int,
	default = multiprocessing.cpu_count(),
	help = "The number of build jobs to run in parallel. Defaults to cpu_count."
)

parser.add_argument(
	"--cleanup",
	action = "store_true",
	help = "Delete archive files after extraction and 'working' directories after each project build completes."
)

for project in __projects() :

	config = __loadJSON( project )
	variants = config.get( "variants" )
	if not variants :
		continue

	parser.add_argument(
		"--variant:{}".format( project ),
		choices = variants,
		nargs = 1,
		default = variants[0],
		help = "The build variant for {}.".format( project )
	)

args = parser.parse_args()

variants = {}
for key, value in vars( args ).items() :
	if key.startswith( "variant:" ) :
		variants[key[8:]] = value[0]

variables = {
	"buildDir" : os.path.abspath( args.buildDir ).replace("\\", "/"),
	"jobs" : args.jobs,
	"path" : os.environ["PATH"],
	"version" : __version,
	"platform" : { "darwin": "macos", "win32": "windows" }.get( sys.platform, "linux" ),
	"sharedLibraryExtension" : { "darwin": ".dylib", "win32": ".dll" }.get( sys.platform, ".so" ),
	"staticLibraryExtension" : ".lib" if sys.platform == "win32" else ".a",
	"pythonModuleExtension" : ".pyd" if sys.platform == "win32" else ".so",
	"executableExtension" : ".exe" if sys.platform == "win32" else "",
	"libraryPrefix" : "" if sys.platform == "win32" else "lib",
	"c++Standard" : "17",
	"compilerRoot" : __compilerRoot(),
	"cmakeGenerator" : "\"NMake Makefiles JOM\"" if sys.platform == "win32" else "\"Unix Makefiles\"",
	"cmakeBuildType": "Release",
	"variants" : "".join( "-{}{}".format( key, variants[key] ) for key in sorted( variants.keys() ) ),
}

configs = __loadConfigs( variables, variants )
if args.projects is None :
	# We don't default to everything in `__projects()`,
	# because projects may have been disabled based on
	# platform/variant.
	args.projects = sorted( configs.keys() )

__checkConfigs( args.projects, configs )

buildDir = variables["buildDir"].format( **variables )
__buildProjects( args.projects, configs, buildDir, args.cleanup )

if args.package :
	__buildPackage( args.projects, configs, buildDir, args.package.format( **variables ) )
