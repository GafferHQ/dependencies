{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/openexr/archive/v2.4.1.tar.gz"

	],

	"license" : "LICENSE.md",

	"dependencies" : [ "Python", "Boost" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"variables" : {

		"pythonModuleDir" : "{pythonLibDir}/python{pythonVersion}/site-packages",

	},

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			# OpenEXR's CMake setup uses GNUInstallDirs, which unhelpfully
			# puts the libraries in `lib64`. Coax them back.
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=TRUE"
			" -D Boost_NO_BOOST_CMAKE=TRUE"
			" -D BOOST_ROOT={buildDir}"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

		"mkdir -p {buildDir}/python",
		"mv {pythonModuleDir}/iex.so {buildDir}/python",
		"mv {pythonModuleDir}/imath.so {buildDir}/python",

	],

	"manifest" : [

		"bin/exrheader",
		"include/OpenEXR",
		"lib/libIlmImf*{sharedLibraryExtension}*",
		"lib/libIex*{sharedLibraryExtension}*",
		"lib/libHalf*{sharedLibraryExtension}*",
		"lib/libIlmThread*{sharedLibraryExtension}*",
		"lib/libImath*{sharedLibraryExtension}*",
		"lib/libPyIex*{sharedLibraryExtension}*",
		"lib/libPyImath*{sharedLibraryExtension}*",

		"python/iex.so",
		"python/imath.so",

	],

	"variant:Python:2" : {

		"platform:osx" : {

			"variables" : {

				# Yep, a totally different location to where it installs for Python 3,
				# and missing the L in Library.
				"pythonModuleDir" : "{buildDir}/ibrary/Python/2.7/site-packages",

			},

		},

	}

}
